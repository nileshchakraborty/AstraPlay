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

    // 1. Initialize video decoder (Xbox WebRTC stream is always H.264)
    ffmpeg_decoder = CHIAKI_NEW(ChiakiFfmpegDecoder);
    ChiakiCodec codec = CHIAKI_CODEC_H264;
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
        m_gssvHost.remove(QStringLiteral("https://"));
        m_gssvHost.remove(QStringLiteral("http://"));
        while (m_gssvHost.endsWith(QLatin1Char('/'))) m_gssvHost.chop(1);
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
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
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
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    // Resolve game titleId (XCloudTitleId) from catalog if productId or name was passed
    QString playTitleId = m_titleId;
    if (m_isCloud) {
        const QJsonArray &titles = XboxAuthManager::instance()->getCloudTitlesJson();
        for (const QJsonValue &v : titles) {
            QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("titleId")).toString().compare(m_titleId, Qt::CaseInsensitive) == 0 ||
                o.value(QStringLiteral("productId")).toString().compare(m_titleId, Qt::CaseInsensitive) == 0 ||
                o.value(QStringLiteral("name")).toString().compare(m_titleId, Qt::CaseInsensitive) == 0) {
                QString tid = o.value(QStringLiteral("titleId")).toString();
                if (!tid.isEmpty()) {
                    playTitleId = tid;
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
            while (m_sessionPath.startsWith(QLatin1Char('/'))) {
                m_sessionPath.remove(0, 1);
            }
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
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString state = doc.object().value(QStringLiteral("state")).toString();
            qDebug() << "Xbox session state:" << state;
            if (state == QStringLiteral("Provisioned") || state == QStringLiteral("Ready")) {
                setupWebRtc();
            } else if (state == QStringLiteral("ReadyToConnect")) {
                if (m_isCloud) {
                    sendConsoleTransferToken();
                } else {
                    setupWebRtc();
                }
            } else if (state == QStringLiteral("Failed")) {
                emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN, tr("Xbox blade provisioning failed."));
            } else if (state == QStringLiteral("WaitingForResources")) {
                SetLoadingMessage(tr("Waiting for available Xbox server blade..."));
                QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
            } else {
                QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
            }
        } else {
            QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
        }
    });
}

void XboxStreamSession::sendConsoleTransferToken()
{
    if (m_transferTokenSent) {
        // If already sent, keep polling until state transitions to Provisioned
        QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
        return;
    }
    m_transferTokenSent = true;
    qDebug() << "Xbox session is ReadyToConnect: requesting console transfer token...";

    XboxAuthManager::instance()->getConsoleTransferToken([this](bool success, const QString &token) {
        if (!success || token.isEmpty()) {
            qWarning() << "Failed to obtain Xbox console transfer token!";
            m_transferTokenSent = false;
            QTimer::singleShot(1000, this, &XboxStreamSession::pollSessionState);
            return;
        }

        qDebug() << "Obtained console transfer token, sending /connect to GSSV...";
        QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/connect"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
        req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
        req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());
        req.setRawHeader("UserToken", token.toUtf8());

        QJsonObject payload;
        payload[QStringLiteral("userToken")] = token;
        payload[QStringLiteral("UserToken")] = token;
        payload[QStringLiteral("msalToken")] = token;

        QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        QNetworkReply *reply = m_nam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray respBytes = reply->readAll();
            qDebug() << "Xbox /connect response:" << httpCode << "body:" << respBytes;
            if (httpCode != 200 && httpCode != 202 && httpCode != 204) {
                qWarning() << "Xbox /connect failed with code" << httpCode;
                m_transferTokenSent = false;
            }
            // State will transition to Provisioned
            QTimer::singleShot(500, this, &XboxStreamSession::pollSessionState);
        });
    });
}

void XboxStreamSession::setupWebRtc()
{
    if (m_peerConnection) return;
    qDebug() << "Xbox setting up WebRTC...";

    rtc::Configuration config;
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");
    config.iceServers.emplace_back("stun:stun.cloudflare.com:3478");
    m_peerConnection = std::make_shared<rtc::PeerConnection>(config);

    // Register callbacks FIRST so that initial negotiation triggers onLocalDescription
    m_peerConnection->onLocalDescription([this](rtc::Description desc) {
        QString sdpStr = QString::fromStdString(desc);
        qDebug() << "Generated local SDP offer:\n" << sdpStr;
        QMetaObject::invokeMethod(this, [this, sdpStr]() {
            sendSdpOffer(sdpStr);
        });
    });

    m_peerConnection->onLocalCandidate([this](rtc::Candidate candidate) {
        QString candStr = QString::fromStdString(candidate.candidate());
        QString midStr = QString::fromStdString(candidate.mid());
        qDebug() << "Local ICE candidate gathered:" << candStr << "mid:" << midStr;
        QMetaObject::invokeMethod(this, [this, candStr, midStr]() {
            m_localCandidates.append(qMakePair(candStr, midStr));
            if (m_remoteDescriptionSet) {
                sendQueuedIceCandidates();
            }
        });
    });

    m_peerConnection->onStateChange([this](rtc::PeerConnection::State state) {
        QMetaObject::invokeMethod(this, [this, state]() {
            qDebug() << "Xbox PeerConnection state changed:" << static_cast<int>(state);
            if (state == rtc::PeerConnection::State::Connected) {
                connected = true;
                emit ConnectedChanged();
                emit GameLaunchCompleted();
                m_keepaliveTimer->start();
            } else if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed) {
                connected = false;
                emit ConnectedChanged();
                emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_TIMEOUT, tr("Xbox WebRTC connection closed."));
            }
        });
    });

    // Setup Video Depacketizer (Annex B NALU parsing)
    m_videoDepacketizer = std::make_shared<rtc::H264RtpDepacketizer>();

    // Add Video Track (recvonly)
    rtc::Description::Video videoDesc("video", rtc::Description::Direction::RecvOnly);
    videoDesc.addH264Codec(97, "profile-level-id=4d001f;packetization-mode=1;level-asymmetry-allowed=1");
    videoDesc.addH264Codec(96, "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1");
    videoDesc.addH264Codec(98, "profile-level-id=42001f;packetization-mode=1;level-asymmetry-allowed=1");
    m_videoTrack = m_peerConnection->addTrack(videoDesc);
    m_videoTrack->setMediaHandler(m_videoDepacketizer);

    m_videoTrack->onFrame([this](rtc::binary frame, rtc::FrameInfo info) {
        Q_UNUSED(info);
        if (ffmpeg_decoder && !frame.empty()) {
            if (!m_firstFrameReceived) {
                m_firstFrameReceived = true;
                QMetaObject::invokeMethod(this, [this]() {
                    emit GameLaunchCompleted();
                });
            }
            chiaki_ffmpeg_decoder_video_sample_cb(reinterpret_cast<uint8_t*>(frame.data()), frame.size(), 0, true, ffmpeg_decoder);
        }
    });

    // Add Audio Track (recvonly)
    rtc::Description::Audio audioDesc("audio", rtc::Description::Direction::RecvOnly);
    audioDesc.addOpusCodec(111);
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
            QMetaObject::invokeMethod(this, [this, data]() {
                handleMessageChannelPayload(data);
            });
        }
    });

    // DataChannel: control (controlV1, ordered)
    rtc::DataChannelInit controlConfig;
    controlConfig.protocol = "controlV1";
    controlConfig.reliability.unordered = false;
    m_controlChannel = m_peerConnection->createDataChannel("control", controlConfig);

    // DataChannel: chat (chatV1, ordered)
    rtc::DataChannelInit chatConfig;
    chatConfig.protocol = "chatV1";
    chatConfig.reliability.unordered = false;
    m_chatChannel = m_peerConnection->createDataChannel("chat", chatConfig);

    // If local description wasn't automatically triggered by data channel creation, trigger it explicitly
    if (!m_peerConnection->localDescription().has_value()) {
        m_peerConnection->setLocalDescription();
    }
}

void XboxStreamSession::sendSdpOffer(const QString &sdp)
{
    if (m_sessionPath.isEmpty()) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/sdp"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QJsonObject payload;
    payload[QStringLiteral("messageType")] = QStringLiteral("offer");
    payload[QStringLiteral("sdp")] = sdp;
    payload[QStringLiteral("requestId")] = QStringLiteral("1");

    QJsonObject chatConfig;
    chatConfig[QStringLiteral("bytesPerSample")] = 2;
    chatConfig[QStringLiteral("expectedClipDurationMs")] = 20;
    QJsonObject format;
    format[QStringLiteral("codec")] = QStringLiteral("opus");
    format[QStringLiteral("container")] = QStringLiteral("webm");
    chatConfig[QStringLiteral("format")] = format;
    chatConfig[QStringLiteral("numChannels")] = 1;
    chatConfig[QStringLiteral("sampleFrequencyHz")] = 24000;

    QJsonObject ver1;
    ver1[QStringLiteral("minVersion")] = 1;
    ver1[QStringLiteral("maxVersion")] = 1;

    QJsonObject verCtrl;
    verCtrl[QStringLiteral("minVersion")] = 1;
    verCtrl[QStringLiteral("maxVersion")] = 3;

    QJsonObject verInput;
    verInput[QStringLiteral("minVersion")] = 1;
    verInput[QStringLiteral("maxVersion")] = 9;

    QJsonObject verReliable;
    verReliable[QStringLiteral("minVersion")] = 9;
    verReliable[QStringLiteral("maxVersion")] = 9;

    QJsonObject config;
    config[QStringLiteral("chatConfiguration")] = chatConfig;
    config[QStringLiteral("chat")] = ver1;
    config[QStringLiteral("control")] = verCtrl;
    config[QStringLiteral("input")] = verInput;
    config[QStringLiteral("message")] = ver1;
    config[QStringLiteral("reliableinput")] = verReliable;
    config[QStringLiteral("unreliableinput")] = verReliable;

    payload[QStringLiteral("configuration")] = config;

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray respBytes = reply->readAll();
        qDebug() << "Xbox SDP offer response:" << httpCode << "body:" << respBytes;
        if (httpCode == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(respBytes);
            QJsonObject rootObj = doc.object();
            QString answerSdp;
            if (rootObj.contains(QStringLiteral("exchangeResponse"))) {
                QJsonValue exVal = rootObj.value(QStringLiteral("exchangeResponse"));
                if (exVal.isString()) {
                    QJsonDocument exDoc = QJsonDocument::fromJson(exVal.toString().toUtf8());
                    answerSdp = exDoc.object().value(QStringLiteral("sdp")).toString();
                } else if (exVal.isObject()) {
                    answerSdp = exVal.toObject().value(QStringLiteral("sdp")).toString();
                }
            }
            if (answerSdp.isEmpty()) {
                answerSdp = rootObj.value(QStringLiteral("sdp")).toString();
            }

            if (!answerSdp.isEmpty() && m_peerConnection) {
                qDebug() << "Successfully extracted server SDP answer from offer response! Setting remote description...";
                m_remoteDescriptionSet = true;
                m_peerConnection->setRemoteDescription(rtc::Description(answerSdp.toStdString(), "answer"));
                sendQueuedIceCandidates();
                pollIceCandidates();
                return;
            }
        }
        pollSdpAnswer();
    });
}

void XboxStreamSession::pollSdpAnswer()
{
    if (m_stopped || m_sessionPath.isEmpty()) return;
    if (m_peerConnection && m_peerConnection->state() == rtc::PeerConnection::State::Connected) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/sdp"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray respBytes = reply->readAll();
        qDebug() << "Xbox poll SDP answer response:" << httpCode << "body:" << respBytes;

        if (httpCode == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(respBytes);
            QJsonObject rootObj = doc.object();

            QString answerSdp;
            if (rootObj.contains(QStringLiteral("exchangeResponse"))) {
                QJsonValue exVal = rootObj.value(QStringLiteral("exchangeResponse"));
                if (exVal.isString()) {
                    QJsonDocument exDoc = QJsonDocument::fromJson(exVal.toString().toUtf8());
                    answerSdp = exDoc.object().value(QStringLiteral("sdp")).toString();
                } else if (exVal.isObject()) {
                    answerSdp = exVal.toObject().value(QStringLiteral("sdp")).toString();
                }
            }
            if (answerSdp.isEmpty()) {
                answerSdp = rootObj.value(QStringLiteral("sdp")).toString();
            }

            if (!answerSdp.isEmpty() && m_peerConnection) {
                qDebug() << "Successfully extracted server SDP answer! Setting remote description...";
                m_remoteDescriptionSet = true;
                m_peerConnection->setRemoteDescription(rtc::Description(answerSdp.toStdString(), "answer"));
                sendQueuedIceCandidates();
                pollIceCandidates();
                return;
            }

            // Only fail if we did NOT get any answer SDP
            if (rootObj.contains(QStringLiteral("errorDetails")) && !rootObj.value(QStringLiteral("errorDetails")).isNull()) {
                QJsonObject errObj = rootObj.value(QStringLiteral("errorDetails")).toObject();
                QString errMsg = errObj.value(QStringLiteral("message")).toString();
                qWarning() << "Xbox SDP exchange failed:" << errMsg;
                emit SessionQuit(CHIAKI_QUIT_REASON_STREAM_CONNECTION_TIMEOUT, tr("Xbox SDP exchange failed: ") + errMsg);
                return;
            }
        }
        if (!m_stopped && m_peerConnection && m_peerConnection->state() != rtc::PeerConnection::State::Connected) {
            QTimer::singleShot(500, this, &XboxStreamSession::pollSdpAnswer);
        }
    });
}

void XboxStreamSession::pollIceCandidates()
{
    if (m_stopped || m_sessionPath.isEmpty()) return;
    if (m_peerConnection && m_peerConnection->state() == rtc::PeerConnection::State::Connected) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/ice"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray respBytes = reply->readAll();
        qDebug() << "Xbox poll ICE candidates response:" << httpCode << "body:" << respBytes;

        if (httpCode == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(respBytes);
            QJsonObject rootObj = doc.object();
            QJsonArray candList;

            if (rootObj.contains(QStringLiteral("exchangeResponse"))) {
                QJsonValue exVal = rootObj.value(QStringLiteral("exchangeResponse"));
                if (exVal.isString()) {
                    QJsonDocument exDoc = QJsonDocument::fromJson(exVal.toString().toUtf8());
                    if (exDoc.isArray()) {
                        candList = exDoc.array();
                    } else if (exDoc.isObject() && exDoc.object().contains(QStringLiteral("candidates"))) {
                        candList = exDoc.object().value(QStringLiteral("candidates")).toArray();
                    } else if (exDoc.isObject() && exDoc.object().contains(QStringLiteral("iceCandidates"))) {
                        candList = exDoc.object().value(QStringLiteral("iceCandidates")).toArray();
                    }
                } else if (exVal.isArray()) {
                    candList = exVal.toArray();
                } else if (exVal.isObject() && exVal.toObject().contains(QStringLiteral("candidates"))) {
                    candList = exVal.toObject().value(QStringLiteral("candidates")).toArray();
                } else if (exVal.isObject() && exVal.toObject().contains(QStringLiteral("iceCandidates"))) {
                    candList = exVal.toObject().value(QStringLiteral("iceCandidates")).toArray();
                }
            } else if (rootObj.contains(QStringLiteral("candidates"))) {
                candList = rootObj.value(QStringLiteral("candidates")).toArray();
            } else if (rootObj.contains(QStringLiteral("iceCandidates"))) {
                candList = rootObj.value(QStringLiteral("iceCandidates")).toArray();
            }

            for (const QJsonValue &cVal : candList) {
                QJsonObject cObj;
                if (cVal.isString()) {
                    cObj = QJsonDocument::fromJson(cVal.toString().toUtf8()).object();
                } else if (cVal.isObject()) {
                    cObj = cVal.toObject();
                }
                QString candidate = cObj.value(QStringLiteral("candidate")).toString();
                QString mid = cObj.value(QStringLiteral("sdpMid")).toString();
                if (mid.isEmpty()) mid = cObj.value(QStringLiteral("mid")).toString();

                if (candidate.isEmpty() || candidate == QStringLiteral("a=end-of-candidates")) continue;

                // Teredo IPv6 candidate translation (RFC 4380) for Azure blades
                QStringList parts = candidate.split(QLatin1Char(' '));
                if (parts.size() > 4 && parts[4].startsWith(QStringLiteral("2001:"))) {
                    QString ip6 = parts[4];
                    QStringList segs = ip6.split(QLatin1Char(':'));
                    if (segs.size() >= 8) {
                        bool ok1 = false, ok2 = false, ok3 = false, ok4 = false, okP = false;
                        QString ipv4_part = segs[6] + segs[7];
                        int b1 = (~ipv4_part.mid(0, 2).toInt(&ok1, 16)) & 0xFF;
                        int b2 = (~ipv4_part.mid(2, 2).toInt(&ok2, 16)) & 0xFF;
                        int b3 = (~ipv4_part.mid(4, 2).toInt(&ok3, 16)) & 0xFF;
                        int b4 = (~ipv4_part.mid(6, 2).toInt(&ok4, 16)) & 0xFF;
                        int port = (~segs[5].toInt(&okP, 16)) & 0xFFFF;
                        if (ok1 && ok2 && ok3 && ok4 && okP) {
                            QString ipv4Addr = QStringLiteral("%1.%2.%3.%4").arg(b1).arg(b2).arg(b3).arg(b4);
                            QString c10 = QStringLiteral("candidate:10 1 UDP 1 %1 9002 typ host").arg(ipv4Addr);
                            QString c11 = QStringLiteral("candidate:11 1 UDP 1 %1 %2 typ host").arg(ipv4Addr).arg(port);
                            qDebug() << "Extracted Teredo IPv4 candidates:" << c10 << c11;
                            if (m_peerConnection) {
                                m_peerConnection->addRemoteCandidate(rtc::Candidate(c10.toStdString(), mid.toStdString()));
                                m_peerConnection->addRemoteCandidate(rtc::Candidate(c11.toStdString(), mid.toStdString()));
                            }
                        }
                    }
                }

                if (m_peerConnection) {
                    qDebug() << "Adding remote ICE candidate from server:" << candidate << "mid:" << mid;
                    m_peerConnection->addRemoteCandidate(rtc::Candidate(candidate.toStdString(), mid.toStdString()));
                }
            }
        }

        if (!m_stopped && m_peerConnection && m_peerConnection->state() != rtc::PeerConnection::State::Connected) {
            QTimer::singleShot(500, this, &XboxStreamSession::pollIceCandidates);
        }
    });
}

void XboxStreamSession::sendQueuedIceCandidates()
{
    if (m_localCandidates.isEmpty() || m_sessionPath.isEmpty() || !m_remoteDescriptionSet) return;

    QUrl url(QStringLiteral("https://") + m_gssvHost + QStringLiteral("/") + m_sessionPath + QStringLiteral("/ice"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    req.setRawHeader("X-Gssv-Client", "XboxComBrowser");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_gsToken).toUtf8());

    QJsonArray candidateStrings;
    while (!m_localCandidates.isEmpty()) {
        auto pair = m_localCandidates.takeFirst();
        QString candStr = pair.first;
        QString mid = pair.second;

        QJsonObject candObj;
        candObj[QStringLiteral("candidate")] = candStr;
        candObj[QStringLiteral("sdpMid")] = mid;
        candObj[QStringLiteral("sdpMLineIndex")] = (mid == QStringLiteral("audio") ? 1 : (mid == QStringLiteral("video") ? 0 : 2));

        candidateStrings.append(QString::fromUtf8(QJsonDocument(candObj).toJson(QJsonDocument::Compact)));
    }

    QJsonObject payload;
    payload[QStringLiteral("candidates")] = candidateStrings;

    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    qDebug() << "Posting local ICE candidates to GSSV:" << body;
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        reply->deleteLater();
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Xbox ICE candidates post response:" << httpCode << "body:" << reply->readAll();
    });
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
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
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
