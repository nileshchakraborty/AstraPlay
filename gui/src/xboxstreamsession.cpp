// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "xboxstreamsession.h"
#include "controllermanager.h"

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
    connect(ControllerManager::GetInstance(), &ControllerManager::AvailableControllersUpdated, this, &XboxStreamSession::SendFeedbackState);

    // 4. Setup timers
    m_inputTimer->setInterval(4);
    connect(m_inputTimer, &QTimer::timeout, this, &XboxStreamSession::onInputTimer);

    m_keepaliveTimer->setInterval(30000);
    connect(m_keepaliveTimer, &QTimer::timeout, this, &XboxStreamSession::onKeepaliveTimer);
}

XboxStreamSession::~XboxStreamSession()
{
    Stop();

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
            emit SessionQuit(CHIAKI_QUIT_REASON_CTRL_UNKNOWN, tr("Failed to retrieve Xbox stream authorization token."));
            return;
        }

        m_gsToken = token;
        m_gssvHost = host;
        requestPlaySession();
    });
}

void XboxStreamSession::Stop()
{
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
    payload[QStringLiteral("titleId")] = m_isCloud ? m_titleId : QString();
    payload[QStringLiteral("serverId")] = m_isCloud ? QString() : m_serverId;
    payload[QStringLiteral("settings")] = settings;
    payload[QStringLiteral("fallbackRegionNames")] = QJsonArray();

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 202) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            m_sessionPath = obj.value(QStringLiteral("sessionPath")).toString();
            pollSessionState();
        } else {
            emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_TIMEOUT, tr("Xbox GSSV stream provision rejected."));
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

    // DataChannel: input (1.0, unordered, unreliable)
    rtc::DataChannelInit inputConfig;
    inputConfig.protocol = "1.0";
    inputConfig.reliability.unordered = true;
    inputConfig.reliability.maxRetransmits = 0;
    m_inputChannel = m_peerConnection->createDataChannel("input", inputConfig);

    m_inputChannel->onOpen([this]() {
        m_inputTimer->start();
    });

    m_inputChannel->onMessage([this](auto msg) {
        if (std::holds_alternative<rtc::binary>(msg)) {
            auto bin = std::get<rtc::binary>(msg);
            if (bin.size() >= 13 && static_cast<uint8_t>(bin[0]) == 128) {
                // Vibration feedback: left / right motor strength
                uint16_t left = (uint8_t(bin[1]) << 8) | uint8_t(bin[2]);
                uint16_t right = (uint8_t(bin[3]) << 8) | uint8_t(bin[4]);
                emit RumbleHapticPushed(qMax(left, right));
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

QByteArray XboxStreamSession::serializeGamepadPacket()
{
    // Format: 14 bytes header + 1 byte count + 22 bytes gamepad frame = 37 bytes
    QByteArray packet(37, 0);
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
    // Map ChiakiControllerState to Xbox bitmask
    uint16_t xbuttons = 0;
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_CROSS) xbuttons |= 16;       // A
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_MOON) xbuttons |= 32;        // B
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_BOX) xbuttons |= 64;         // X
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_PYRAMID) xbuttons |= 128;    // Y
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_UP) xbuttons |= 256;    // Dpad Up
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN) xbuttons |= 512;  // Dpad Down
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT) xbuttons |= 1024; // Dpad Left
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT) xbuttons |= 2048;// Dpad Right
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_L1) xbuttons |= 4096;        // LB
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_R1) xbuttons |= 8192;        // RB
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_L3) xbuttons |= 16384;       // L3
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_R3) xbuttons |= 32768;       // R3
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_OPTIONS) xbuttons |= 4;      // Menu
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_SHARE) xbuttons |= 8;        // View
    if (keyboard_state.buttons & CHIAKI_CONTROLLER_BUTTON_PS) xbuttons |= 2;           // Nexus

    p[16] = uint8_t(xbuttons & 0xFF);
    p[17] = uint8_t((xbuttons >> 8) & 0xFF);

    // 7. Sticks (int16 little endian)
    int16_t lx = keyboard_state.left_x;
    int16_t ly = -keyboard_state.left_y;
    int16_t rx = keyboard_state.right_x;
    int16_t ry = -keyboard_state.right_y;

    memcpy(p + 18, &lx, 2);
    memcpy(p + 20, &ly, 2);
    memcpy(p + 22, &rx, 2);
    memcpy(p + 24, &ry, 2);

    // 8. Triggers (uint16 little endian 0..65535)
    uint16_t lt = (keyboard_state.l2_state * 65535) / 255;
    uint16_t rt = (keyboard_state.r2_state * 65535) / 255;
    memcpy(p + 26, &lt, 2);
    memcpy(p + 28, &rt, 2);

    // 9. Physicality flags (uint32 = 1)
    uint32_t one = 1;
    memcpy(p + 30, &one, 4);

    return packet;
}

void XboxStreamSession::SendFeedbackState()
{
    // Aggregate input states from ControllerManager into keyboard_state
    // onInputTimer() transmits every 4ms
}

void XboxStreamSession::HandleKeyboardEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
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
