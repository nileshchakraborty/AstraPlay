// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include "streamsession.h"
#include "sessionlog.h"
#include "xboxauthmanager.h"
#include "controllermanager.h"

#include <rtc/rtc.hpp>
#include <chiaki/opusdecoder.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMap>
#include <memory>

class XboxStreamSession : public StreamSession
{
    Q_OBJECT

public:
    explicit XboxStreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent = nullptr);
    ~XboxStreamSession() override;

    bool IsConnected() override { return connected; }
    bool IsStarted() const override { return session_started; }
    bool IsConnecting() override { return connect_timer.isValid(); }

    void Start() override;
    void Stop() override;
    void GoToBed() override;
    void ToggleMute() override;
    void GoHome() override;

    QString GetResolution() override;
    bool IsCloudStreaming() override { return m_isCloud; }

    void HandleKeyboardEvent(QKeyEvent *event) override;
    void HandleTouchEvent(QTouchEvent *event, qreal width, qreal height) override;
    void HandleDpadTouchEvent(ChiakiControllerState *state, bool placeholder = false) override;
    void HandleMouseReleaseEvent(QMouseEvent *event) override;
    void HandleMousePressEvent(QMouseEvent *event) override;
    void HandleMouseMoveEvent(QMouseEvent *event, qreal width, qreal height) override;
    void ReadMic(const QByteArray &micdata) override;

    void SendFeedbackState() override;

    StreamBackend GetBackend() const override { return StreamBackend::Xbox; }

private slots:
    void onInputTimer();
    void onKeepaliveTimer();

private:
    SessionLog m_log;
    QNetworkAccessManager *m_nam;

    bool m_isCloud = false;
    bool m_transferTokenSent = false;
    QString m_gssvHost;
    QString m_gsToken;
    QString m_sessionPath;
    QString m_serverId;
    QString m_titleId;

    // WebRTC PeerConnection and DataChannels
    std::shared_ptr<rtc::PeerConnection> m_peerConnection;
    std::shared_ptr<rtc::DataChannel> m_inputChannel;
    std::shared_ptr<rtc::DataChannel> m_messageChannel;
    std::shared_ptr<rtc::DataChannel> m_controlChannel;
    std::shared_ptr<rtc::DataChannel> m_chatChannel;
    std::shared_ptr<rtc::Track> m_videoTrack;
    std::shared_ptr<rtc::Track> m_audioTrack;

    // Video depacketization & decoding
    std::shared_ptr<rtc::H264RtpDepacketizer> m_videoDepacketizer;

    // Audio decoding
    ChiakiOpusDecoder m_opusDecoder;

    // Timers
    QTimer *m_inputTimer;
    QTimer *m_keepaliveTimer;
    uint32_t m_inputSequence = 0;

    // GSSV HTTP Provisioning Steps
    void requestPlaySession();
    void pollSessionState();
    void setupWebRtc();
    void sendSdpOffer(const QString &sdp);
    void pollSdpAnswer();
    void pollIceCandidates();
    void sendQueuedIceCandidates();
    void sendConsoleTransferToken();
    void handleMessageChannelPayload(const QByteArray &data);
    void sendSystemUiConfigurations();

    bool m_remoteDescriptionSet = false;
    bool m_firstFrameReceived = false;
    QList<QPair<QString, QString>> m_localCandidates;

    // Gamepad binary frame serialization (38-byte binary layout)
    QByteArray serializeGamepadPacket();

    // Physical controllers
    void UpdateControllers();

    bool m_stopped = false;

    friend class XboxAuthTest;
};
