// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "xboxstreamsession.h"
#include "controllermanager.h"
#include "settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUuid>
#include <QDateTime>
#include <QtEndian>
#include <QDebug>

#include <chiaki/common.h>
#include <chiaki/ffmpegdecoder.h>
#include <chiaki/time.h>

static void FfmpegFrameCb(ChiakiFfmpegDecoder *decoder, void *user)
{
    Q_UNUSED(decoder);
    auto session = reinterpret_cast<XboxStreamSession *>(user);
    QMetaObject::invokeMethod(session, "FfmpegFrameAvailable", Qt::QueuedConnection);
}

XboxStreamSession::XboxStreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent)
    : StreamSession(connect_info, parent),
      m_log(this, connect_info.log_level_mask, connect_info.log_file),
      m_nam(new QNetworkAccessManager(this)),
      m_inputTimer(new QTimer(this)),
      m_keepaliveTimer(new QTimer(this))
{
    m_isCloud = !connect_info.title_id.isEmpty() && connect_info.duid.isEmpty();
    m_titleId = connect_info.title_id;
    m_serverId = connect_info.duid; // duid holds Xbox serverId for xHome

    // 1. Initialize video decoder
    ffmpeg_decoder = CHIAKI_NEW(ChiakiFfmpegDecoder);
    ChiakiCodec codec = connect_info.video_profile.codec;
    chiaki_ffmpeg_decoder_init(ffmpeg_decoder,
                               m_log.GetChiakiLog(),
                               codec,
                               connect_info.hw_decoder.isEmpty() ? nullptr : connect_info.hw_decoder.toUtf8().constData(),
                               connect_info.hw_device_ctx,
                               FfmpegFrameCb,
                               this);

    // 2. Initialize Opus audio decoder
    chiaki_opus_decoder_init(&m_opusDecoder, m_log.GetChiakiLog());

    // 3. Connect controller updates
    connect(ControllerManager::GetInstance(), &ControllerManager::AvailableControllersUpdated, this, &XboxStreamSession::UpdateControllers);
    UpdateControllers();

    // 4. Setup timers
    m_inputTimer->setInterval(4);
    connect(m_inputTimer, &QTimer::timeout, this, &XboxStreamSession::onInputTimer);

    m_keepaliveTimer->setInterval(30000);
    connect(m_keepaliveTimer, &QTimer::timeout, this, &XboxStreamSession::onKeepaliveTimer);
}

XboxStreamSession::~XboxStreamSession()
{
    m_stopped = true;
    disconnect();
    Stop();

    for (auto controller : controllers) {
        controller->Unref();
    }
    controllers.clear();

    if (ffmpeg_decoder) {
        chiaki_ffmpeg_decoder_fini(ffmpeg_decoder);
        free(ffmpeg_decoder);
        ffmpeg_decoder = nullptr;
    }

    chiaki_opus_decoder_fini(&m_opusDecoder);
}

void XboxStreamSession::Start()
{
    connect_timer.start();
    session_started = true;
    SetLoadingMessage(tr("Connecting to Xbox Stream..."));

    QString offering = m_isCloud ? QStringLiteral("xgpuweb") : QStringLiteral("xhome");
    XboxAuthManager::instance()->getStreamingToken(offering, [this](bool success, const QString &token, const QString &host) {
        if (!success || token.isEmpty()) {
            m_stopped = true;
            emit SessionQuit(CHIAKI_QUIT_REASON_CTRL_UNKNOWN,
                             m_isCloud ? tr("Xbox Cloud Gaming requires an active Xbox Game Pass Ultimate subscription.")
                                       : tr("Failed to retrieve Xbox stream authorization token. Ensure Remote features are enabled on your console."));
            return;
        }

        m_gsToken = token;
        m_gssvHost = host;
        requestPlaySession();
    });
}

void XboxStreamSession::Stop()
{
    if (m_stopped)
        return;
    m_stopped = true;

    m_inputTimer->stop();
    m_keepaliveTimer->stop();

    if (!m_sessionPath.isEmpty() && !m_gssvHost.isEmpty() && !m_gsToken.isEmpty()) {
        QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath);
        QNetworkRequest req(url);
        req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
        req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());
        QNetworkReply *reply = m_nam->deleteResource(req);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
        m_sessionPath.clear();
    }

    if (m_peerConnection) {
        m_peerConnection->close();
        m_peerConnection.reset();
    }

    connected = false;
    session_started = false;
    emit ConnectedChanged();
    emit SessionQuit(CHIAKI_QUIT_REASON_NONE, QString());
}

void XboxStreamSession::GoToBed()
{
    if (!m_serverId.isEmpty()) {
        XboxAuthManager::instance()->turnOffConsole(m_serverId);
    }
    Stop();
}

void XboxStreamSession::GoHome()
{
    // Toggle Nexus / Xbox Guide button (bit 2)
    keyboard_state.buttons ^= 2;
    onInputTimer();
    QTimer::singleShot(100, this, [this]() {
        keyboard_state.buttons &= ~2;
        onInputTimer();
    });
}

void XboxStreamSession::ToggleMute()
{
    muted = !muted;
    emit MutedChanged();
}

QString XboxStreamSession::GetResolution()
{
    return QStringLiteral("1920x1080");
}

void XboxStreamSession::requestPlaySession()
{
    QString playPath = QStringLiteral("/v5/sessions/") + (m_isCloud ? QStringLiteral("cloud") : QStringLiteral("home")) + QStringLiteral("/play");
    QUrl url(QStringLiteral("https://") + m_gssvHost + playPath);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    // Resolve game product ID from catalog if titleId was passed as a name or slug
    QString playTitleId = m_titleId;
    if (m_isCloud) {
        const QJsonArray &titles = XboxAuthManager::instance()->getCloudTitlesJson();
        for (const QJsonValue &v : titles) {
            QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("titleId")).toString().compare(m_titleId, Qt::CaseInsensitive) == 0 ||
                o.value(QStringLiteral("productId")).toString().compare(m_titleId, Qt::CaseInsensitive) == 0 ||
                o.value(QStringLiteral("name")).toString().compare(m_titleId, Qt::CaseInsensitive) == 0) {
                QString pid = o.value(QStringLiteral("productId")).toString();
                if (!pid.isEmpty()) {
                    playTitleId = pid;
                }
                break;
            }
        }
    }

    QJsonObject settings;
    settings[QStringLiteral("nanoVersion")] = QStringLiteral("V3;WebrtcTransport.dll");
    settings[QStringLiteral("enableOptionalDataCollection")] = false;
    settings[QStringLiteral("enableTextToSpeech")] = false;
    settings[QStringLiteral("highContrast")] = 0;
    settings[QStringLiteral("locale")] = QStringLiteral("en-US");
    settings[QStringLiteral("useIceConnection")] = false;
    settings[QStringLiteral("timezoneOffsetMinutes")] = 0;
    settings[QStringLiteral("sdkType")] = QStringLiteral("web");
    settings[QStringLiteral("osName")] = QStringLiteral("windows");

    QJsonObject payload;
    payload[QStringLiteral("clientSessionId")] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    payload[QStringLiteral("titleId")] = m_isCloud ? playTitleId : QString();
    payload[QStringLiteral("systemUpdateGroup")] = QString();
    payload[QStringLiteral("serverId")] = m_isCloud ? QString() : m_serverId;
    payload[QStringLiteral("settings")] = settings;
    payload[QStringLiteral("fallbackRegionNames")] = QJsonArray();

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray respData = reply->readAll();
        qDebug() << "Xbox GSSV play response:" << httpCode << "for URL:" << url.toString() << "body:" << respData;

        if (httpCode == 200 || httpCode == 202) {
            QJsonDocument doc = QJsonDocument::fromJson(respData);
            QJsonObject obj = doc.object();
            m_sessionPath = obj.value(QStringLiteral("sessionPath")).toString();
            pollSessionState();
        } else {
            QString detail;
            QJsonDocument errDoc = QJsonDocument::fromJson(respData);
            if (errDoc.isObject() && errDoc.object().contains(QStringLiteral("message"))) {
                detail = errDoc.object().value(QStringLiteral("message")).toString();
            } else if (!respData.isEmpty()) {
                detail = QString::fromUtf8(respData);
            }

            QString errorMsg = tr("Xbox GSSV stream provision rejected.");
            if (!detail.isEmpty()) {
                errorMsg += QStringLiteral(" (%1)").arg(detail);
            } else if (httpCode > 0) {
                errorMsg += QStringLiteral(" (HTTP %1)").arg(httpCode);
            }
            emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_TIMEOUT, errorMsg);
        }
    });
}

void XboxStreamSession::pollSessionState()
{
    if (m_sessionPath.isEmpty()) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/state"));
    QNetworkRequest req(url);
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString state = doc.object().value(QStringLiteral("state")).toString();
            if (state == QStringLiteral("Provisioned") || state == QStringLiteral("Ready")) {
                setupWebRtc();
            } else if (state == QStringLiteral("Failed")) {
                emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN, tr("Xbox blade provisioning failed."));
            } else {
                QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
            }
        } else {
            QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
        }
    });
}

void XboxStreamSession::setupWebRtc()
{
    rtc::Configuration config;
    m_peerConnection = std::make_shared<rtc::PeerConnection>(config);

    // Setup Video Depacketizer (Annex B NALU parsing)
    m_videoDepacketizer = std::make_shared<rtc::H264RtpDepacketizer>();

    // Add Video Track (recvonly)
    rtc::Description::Video videoDesc("video", rtc::Description::Direction::RecvOnly);
    videoDesc.addH264Codec(96);
    m_videoTrack = m_peerConnection->addTrack(videoDesc);
    m_videoTrack->setMediaHandler(m_videoDepacketizer);

    m_videoTrack->onFrame([this](rtc::binary frame, rtc::FrameInfo info) {
        Q_UNUSED(info);
        if (ffmpeg_decoder && !frame.empty()) {
            chiaki_ffmpeg_decoder_video_sample_cb(reinterpret_cast<uint8_t*>(frame.data()), frame.size(), 0, true, ffmpeg_decoder);
        }
    });

    // Add Audio Track (sendrecv)
    rtc::Description::Audio audioDesc("audio", rtc::Description::Direction::SendRecv);
    audioDesc.addOpusCodec(97);
    m_audioTrack = m_peerConnection->addTrack(audioDesc);

    // DataChannel: input (1.0, ordered)
    rtc::DataChannelInit inputConfig;
    inputConfig.protocol = "1.0";
    inputConfig.reliability.unordered = false;
    m_inputChannel = m_peerConnection->createDataChannel("input", inputConfig);

    m_inputChannel->onOpen([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            m_inputTimer->start();
        });
    });

    m_inputChannel->onMessage([this](auto msg) {
        if (std::holds_alternative<rtc::binary>(msg)) {
            auto bin = std::get<rtc::binary>(msg);
            if (bin.size() >= 13) {
                uint16_t reportType = static_cast<uint8_t>(bin[0]) | (static_cast<uint8_t>(bin[1]) << 8);
                if (reportType == 128) { // ReportTypes.Vibration
                    uint8_t leftPercent = static_cast<uint8_t>(bin[4]);
                    uint8_t rightPercent = static_cast<uint8_t>(bin[5]);
                    uint16_t left = (static_cast<uint32_t>(leftPercent) * 65535) / 100;
                    uint16_t right = (static_cast<uint32_t>(rightPercent) * 65535) / 100;
                    QMetaObject::invokeMethod(this, [this, left, right]() {
                        emit RumbleHapticPushed(qMax(left, right));
                        for (auto controller : controllers) {
                            controller->SetHapticRumble(left, right);
                            controller->SetRumble(static_cast<uint8_t>(left >> 8), static_cast<uint8_t>(right >> 8));
                        }
                    });
                }
            }
        }
    });

    // DataChannel: message (messageV1, ordered)
    rtc::DataChannelInit messageConfig;
    messageConfig.protocol = "messageV1";
    messageConfig.reliability.unordered = false;
    m_messageChannel = m_peerConnection->createDataChannel("message", messageConfig);

    m_messageChannel->onOpen([this]() {
        QJsonObject handshake;
        handshake[QStringLiteral("type")] = QStringLiteral("Handshake");
        handshake[QStringLiteral("version")] = QStringLiteral("messageV1");
        handshake[QStringLiteral("id")] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        handshake[QStringLiteral("cv")] = QStringLiteral("0");
        QByteArray data = QJsonDocument(handshake).toJson(QJsonDocument::Compact);
        m_messageChannel->send(data.toStdString());
    });

    m_messageChannel->onMessage([this](auto msg) {
        if (std::holds_alternative<std::string>(msg)) {
            QByteArray data = QByteArray::fromStdString(std::get<std::string>(msg));
            handleMessageChannelPayload(data);
        }
    });

    // DataChannel: control (controlV1, ordered)
    rtc::DataChannelInit controlConfig;
    controlConfig.protocol = "controlV1";
    controlConfig.reliability.unordered = false;
    m_controlChannel = m_peerConnection->createDataChannel("control", controlConfig);

    // ICE Candidate callback
    m_peerConnection->onLocalCandidate([this](rtc::Candidate candidate) {
        sendIceCandidate(QString::fromStdString(candidate.candidate()), QString::fromStdString(candidate.mid()));
    });

    m_peerConnection->onStateChange([this](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected) {
            connected = true;
            emit ConnectedChanged();
            m_keepaliveTimer->start();
        } else if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed) {
            connected = false;
            emit ConnectedChanged();
            emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_TIMEOUT, tr("Xbox WebRTC connection closed."));
        }
    });

    // Generate SDP offer
    m_peerConnection->setLocalDescription();
    m_peerConnection->onLocalDescription([this](rtc::Description desc) {
        sendSdpOffer(QString::fromStdString(desc));
    });
}

void XboxStreamSession::sendSdpOffer(const QString &sdp)
{
    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/sdp"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QJsonObject payload;
    payload[QStringLiteral("messageType")] = QStringLiteral("offer");
    payload[QStringLiteral("sdp")] = sdp;
    payload[QStringLiteral("requestId")] = QStringLiteral("1");

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        pollSdpAnswer();
    });
}

void XboxStreamSession::pollSdpAnswer()
{
    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/sdp"));
    QNetworkRequest req(url);
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString answerSdp = doc.object().value(QStringLiteral("sdp")).toString();
            if (!answerSdp.isEmpty() && m_peerConnection) {
                m_peerConnection->setRemoteDescription(rtc::Description(answerSdp.toStdString(), "answer"));
            }
        }
    });
}

void XboxStreamSession::sendIceCandidate(const QString &candidate, const QString &mid)
{
    if (m_sessionPath.isEmpty()) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/ice"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QJsonObject candObj;
    candObj[QStringLiteral("candidate")] = candidate;
    candObj[QStringLiteral("sdpMid")] = mid;

    QJsonArray candidates;
    candidates.append(candObj);

    QJsonObject payload;
    payload[QStringLiteral("candidates")] = candidates;

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void XboxStreamSession::handleMessageChannelPayload(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QString type = doc.object().value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("HandshakeAck")) {
        sendSystemUiConfigurations();

        // Send Control Channel authorization
        if (m_controlChannel && m_controlChannel->isOpen()) {
            QJsonObject authReq;
            authReq[QStringLiteral("message")] = QStringLiteral("authorizationRequest");
            authReq[QStringLiteral("accessKey")] = QStringLiteral("4BDB3609-C1F1-4195-9B37-FEFF45DA8B8E");
            m_controlChannel->send(QJsonDocument(authReq).toJson(QJsonDocument::Compact).toStdString());

            QJsonObject gpChanged;
            gpChanged[QStringLiteral("message")] = QStringLiteral("gamepadChanged");
            gpChanged[QStringLiteral("gamepadIndex")] = 0;
            gpChanged[QStringLiteral("wasAdded")] = true;
            m_controlChannel->send(QJsonDocument(gpChanged).toJson(QJsonDocument::Compact).toStdString());
        }
    }
}

void XboxStreamSession::sendSystemUiConfigurations()
{
    if (!m_messageChannel || !m_messageChannel->isOpen()) return;

    auto sendMsg = [this](const QString &target, const QJsonObject &content) {
        QJsonObject envelope;
        envelope[QStringLiteral("type")] = QStringLiteral("Message");
        envelope[QStringLiteral("target")] = target;
        envelope[QStringLiteral("content")] = QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
        envelope[QStringLiteral("id")] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        envelope[QStringLiteral("cv")] = QStringLiteral("0");
        m_messageChannel->send(QJsonDocument(envelope).toJson(QJsonDocument::Compact).toStdString());
    };

    // 1. UI configuration
    QJsonObject uiConfig;
    QJsonArray ver; ver.append(0); ver.append(2); ver.append(0);
    uiConfig[QStringLiteral("version")] = ver;
    uiConfig[QStringLiteral("systemUis")] = QJsonArray();
    sendMsg(QStringLiteral("/streaming/systemUi/configuration"), uiConfig);

    // 2. Client App Install ID
    QJsonObject clientApp;
    clientApp[QStringLiteral("clientAppInstallId")] = QStringLiteral("c97d7ee0-73b2-4239-bf1d-9d805a338429");
    sendMsg(QStringLiteral("/streaming/properties/clientappinstallidchanged"), clientApp);

    // 3. Orientation
    QJsonObject orient;
    orient[QStringLiteral("orientation")] = 0;
    sendMsg(QStringLiteral("/streaming/characteristics/orientationchanged"), orient);

    // 4. Touch input disabled
    QJsonObject touch;
    touch[QStringLiteral("touchInputEnabled")] = false;
    sendMsg(QStringLiteral("/streaming/characteristics/touchinputenabledchanged"), touch);

    // 5. Dimensions (1080p)
    QJsonObject dims;
    dims[QStringLiteral("horizontal")] = 1920;
    dims[QStringLiteral("vertical")] = 1080;
    dims[QStringLiteral("preferredWidth")] = 1920;
    dims[QStringLiteral("preferredHeight")] = 1080;
    dims[QStringLiteral("safeAreaLeft")] = 0;
    dims[QStringLiteral("safeAreaTop")] = 0;
    dims[QStringLiteral("safeAreaRight")] = 1920;
    dims[QStringLiteral("safeAreaBottom")] = 1080;
    sendMsg(QStringLiteral("/streaming/characteristics/dimensionschanged"), dims);
}

void XboxStreamSession::onKeepaliveTimer()
{
    if (m_sessionPath.isEmpty()) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/keepalive"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QNetworkReply *reply = m_nam->post(req, "{}");
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void XboxStreamSession::onInputTimer()
{
    if (!m_inputChannel || !m_inputChannel->isOpen()) return;

    QByteArray packet = serializeGamepadPacket();
    m_inputChannel->send(reinterpret_cast<const std::byte*>(packet.constData()), packet.size());
}

void XboxStreamSession::UpdateControllers()
{
#if CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
    for (auto controller_id : controllers.keys()) {
        auto controller = controllers[controller_id];
        if (!controller->IsConnected()) {
            CHIAKI_LOGI(m_log.GetChiakiLog(), "XboxStreamSession: Controller %d disconnected", controller->GetDeviceID());
            controllers.remove(controller_id);
            controller->Unref();
        }
    }

    const auto available_controllers = ControllerManager::GetInstance()->GetAvailableControllers();
    for (auto controller_id : available_controllers) {
        if (!controllers.contains(controller_id)) {
            auto controller = ControllerManager::GetInstance()->OpenController(controller_id);
            if (!controller) {
                CHIAKI_LOGE(m_log.GetChiakiLog(), "XboxStreamSession: Failed to open controller %d", controller_id);
                continue;
            }
            CHIAKI_LOGI(m_log.GetChiakiLog(), "XboxStreamSession: Controller %d opened: \"%s\"", controller_id, controller->GetName().toLocal8Bit().constData());
            connect(controller, &Controller::StateChanged, this, &XboxStreamSession::SendFeedbackState);
            controllers[controller_id] = controller;
        }
    }
#endif
}

QByteArray XboxStreamSession::serializeGamepadPacket()
{
    // 1. Aggregate physical controllers and keyboard state
    ChiakiControllerState combined_state;
    chiaki_controller_state_set_idle(&combined_state);

    for (auto controller : controllers) {
        auto c_state = controller->GetState();
        chiaki_controller_state_or(&combined_state, &combined_state, &c_state);
    }
    chiaki_controller_state_or(&combined_state, &combined_state, &keyboard_state);

    // Format: 14 bytes header + 1 byte count + 23 bytes gamepad frame = 38 bytes
    QByteArray packet(38, 0);
    uint8_t *p = reinterpret_cast<uint8_t*>(packet.data());

    // 1. ReportType: Gamepad = 2 (uint16 little endian)
    p[0] = 0x02;
    p[1] = 0x00;

    // 2. Sequence (uint32 little endian)
    m_inputSequence++;
    p[2] = uint8_t(m_inputSequence & 0xFF);
    p[3] = uint8_t((m_inputSequence >> 8) & 0xFF);
    p[4] = uint8_t((m_inputSequence >> 16) & 0xFF);
    p[5] = uint8_t((m_inputSequence >> 24) & 0xFF);

    // 3. Timestamp (double / float64 little endian ms)
    double nowMs = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    memcpy(p + 6, &nowMs, sizeof(double));

    // 4. Gamepad Frames Count = 1
    p[14] = 0x01;

    // 5. Gamepad Index = 0
    p[15] = 0x00;

    // 6. Buttons bitmask (uint16 little endian)
    // Map ChiakiControllerState to Xbox WebRTC bitmask:
    // Nexus: 2, Menu: 4, View: 8, A: 16, B: 32, X: 64, Y: 128,
    // DPadUp: 256, DPadDown: 512, DPadLeft: 1024, DPadRight: 2048,
    // LB: 4096, RB: 8192, L3: 16384, R3: 32768
    uint16_t xbuttons = 0;
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_CROSS) xbuttons |= 16;        // A
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_MOON) xbuttons |= 32;         // B
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_BOX) xbuttons |= 64;          // X
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_PYRAMID) xbuttons |= 128;     // Y
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_UP) xbuttons |= 256;     // Dpad Up
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN) xbuttons |= 512;   // Dpad Down
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT) xbuttons |= 1024;  // Dpad Left
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT) xbuttons |= 2048; // Dpad Right
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_L1) xbuttons |= 4096;         // LB
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_R1) xbuttons |= 8192;         // RB
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_L3) xbuttons |= 16384;        // L3
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_R3) xbuttons |= 32768;        // R3
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_OPTIONS) xbuttons |= 4;       // Menu
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_SHARE) xbuttons |= 8;         // View
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_PS) xbuttons |= 2;            // Nexus
    if (combined_state.buttons & CHIAKI_CONTROLLER_BUTTON_TOUCHPAD) xbuttons |= 8;      // Touchpad as View

    p[16] = uint8_t(xbuttons & 0xFF);
    p[17] = uint8_t((xbuttons >> 8) & 0xFF);

    // 7. Sticks (int16 little endian, Y inverted: up is negative in Chiaki, positive in Xbox)
    int16_t lx = combined_state.left_x;
    int16_t ly = -combined_state.left_y;
    int16_t rx = combined_state.right_x;
    int16_t ry = -combined_state.right_y;

    memcpy(p + 18, &lx, 2);
    memcpy(p + 20, &ly, 2);
    memcpy(p + 22, &rx, 2);
    memcpy(p + 24, &ry, 2);

    // 8. Triggers (uint16 little endian 0..65535)
    uint16_t lt = static_cast<uint16_t>((combined_state.l2_state * 65535) / 255);
    uint16_t rt = static_cast<uint16_t>((combined_state.r2_state * 65535) / 255);
    memcpy(p + 26, &lt, 2);
    memcpy(p + 28, &rt, 2);

    // 9. PhysicalPhysicality (uint32 little endian = 1)
    uint32_t physical = 1;
    memcpy(p + 30, &physical, 4);

    // 10. VirtualPhysicality (uint32 big endian = 1)
    p[34] = 0;
    p[35] = 0;
    p[36] = 0;
    p[37] = 1;

    return packet;
}

void XboxStreamSession::SendFeedbackState()
{
    if (m_inputChannel && m_inputChannel->isOpen()) {
        QByteArray packet = serializeGamepadPacket();
        m_inputChannel->send(reinterpret_cast<const std::byte*>(packet.constData()), packet.size());
    }
}

void XboxStreamSession::HandleKeyboardEvent(QKeyEvent *event)
{
    if (!keyboard_controller_enabled)
        return;
    if (!key_map.contains(Qt::Key(event->key())))
        return;
    if (event->isAutoRepeat())
        return;

    int button = key_map[Qt::Key(event->key())];
    bool press_event = event->type() == QEvent::Type::KeyPress;

    switch (button) {
        case CHIAKI_CONTROLLER_ANALOG_BUTTON_L2:
            keyboard_state.l2_state = press_event ? 0xff : 0;
            break;
        case CHIAKI_CONTROLLER_ANALOG_BUTTON_R2:
            keyboard_state.r2_state = press_event ? 0xff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_UP):
            keyboard_state.right_y = press_event ? -0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_DOWN):
            keyboard_state.right_y = press_event ? 0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_UP):
            keyboard_state.right_x = press_event ? 0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_DOWN):
            keyboard_state.right_x = press_event ? -0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_UP):
            keyboard_state.left_y = press_event ? -0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_DOWN):
            keyboard_state.left_y = press_event ? 0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_UP):
            keyboard_state.left_x = press_event ? 0x7fff : 0;
            break;
        case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_DOWN):
            keyboard_state.left_x = press_event ? -0x7fff : 0;
            break;
        default:
            if (press_event)
                keyboard_state.buttons |= button;
            else
                keyboard_state.buttons &= ~button;
            break;
    }

    SendFeedbackState();
}

void XboxStreamSession::HandleTouchEvent(QTouchEvent *event, qreal width, qreal height)
{
    Q_UNUSED(event); Q_UNUSED(width); Q_UNUSED(height);
}

void XboxStreamSession::HandleDpadTouchEvent(ChiakiControllerState *state, bool placeholder)
{
    Q_UNUSED(state); Q_UNUSED(placeholder);
}

void XboxStreamSession::HandleMouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void XboxStreamSession::HandleMousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void XboxStreamSession::HandleMouseMoveEvent(QMouseEvent *event, qreal width, qreal height)
{
    Q_UNUSED(event); Q_UNUSED(width); Q_UNUSED(height);
}

void XboxStreamSession::ReadMic(const QByteArray &micdata)
{
    Q_UNUSED(micdata);
}
