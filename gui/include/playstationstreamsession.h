// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PLAYSTATIONSTREAMSESSION_H
#define CHIAKI_PLAYSTATIONSTREAMSESSION_H

#include "streamsession.h"
#include "sessionlog.h"

#include <chiaki/session.h>
#include <chiaki/opusdecoder.h>
#include <chiaki/opusencoder.h>
#include <chiaki/streamconnection.h>
#include <chiaki/remote/holepunch.h>

#if CHIAKI_GUI_ENABLE_SETSU
#include <setsu.h>
#include <chiaki/orientation.h>
#endif

#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
#include <sdeck.h>
#include <chiaki/orientation.h>
#endif

#if CHIAKI_GUI_ENABLE_SPEEX
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#endif

class PlayStationStreamSession : public StreamSession
{
	friend class PlayStationStreamSessionPrivate;
	friend class GameLauncher;

	Q_OBJECT

	private:
		SessionLog log;
		ChiakiSession session;
		ChiakiOpusDecoder opus_decoder;
		ChiakiOpusEncoder opus_encoder;
		ChiakiServiceType service_type;
		int mid_stream_reconnect_attempts = 0;
		int haptics_handheld;
		float rumble_multiplier;
		int ps5_rumble_intensity;
		int ps5_trigger_intensity;
		uint8_t led_color[3];
		uint8_t player_index;
		QQueue<uint16_t> rumble_haptics;
		bool rumble_haptics_connected;
		bool rumble_haptics_on;
		float PS_TOUCHPAD_MAX_X, PS_TOUCHPAD_MAX_Y;
		ChiakiControllerState touch_state;
		QMap<int, uint8_t> touch_tracker;
		int8_t mouse_touch_id;
		ChiakiControllerState dpad_touch_state;
		uint16_t dpad_touch_increment;
		float trigger_override;
		float haptic_override;
		bool dpad_regular;
		bool dpad_regular_touch_switched;
		uint dpad_touch_shortcut1;
		uint dpad_touch_shortcut2;
		uint dpad_touch_shortcut3;
		uint dpad_touch_shortcut4;
		int8_t dpad_touch_id;
		QPair<uint16_t, uint16_t> dpad_touch_value;
		QTimer *dpad_touch_timer, *dpad_touch_stop_timer;
		QElapsedTimer double_tap_timer;
		RumbleHapticsIntensity rumble_haptics_intensity;
		SDL_AudioDeviceID haptics_output;
		quint64 haptics_retry_generation = 0;
		size_t haptics_retry_attempt = 0;
		uint8_t *haptics_resampler_buf;
		size_t haptics_buffer_size;
		ChiakiHolepunchSession holepunch_session;

#if CHIAKI_GUI_ENABLE_SETSU
		Setsu *setsu;
		QMap<QPair<QString, SetsuTrackingId>, uint8_t> setsu_ids;
		ChiakiControllerState setsu_state;
		SetsuDevice *setsu_motion_device;
		ChiakiOrientationTracker orient_tracker;
		ChiakiAccelNewZero setsu_accel_zero, setsu_real_accel;
		bool orient_dirty;
#endif

#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
		SDeck *sdeck;
		ChiakiControllerState sdeck_state;
		QQueue<haptic_packet_t> sdeck_hapticl;
		QQueue<haptic_packet_t> sdeck_hapticr;
		int16_t *sdeck_haptics_senderl;
		int16_t *sdeck_haptics_senderr;
		int sdeck_queue_segment;
		uint64_t sdeck_last_haptic;
		bool sdeck_skipl, sdeck_skipr;
		bool enable_steamdeck_haptics;
		ChiakiOrientationTracker sdeck_orient_tracker;
		ChiakiAccelNewZero sdeck_accel_zero, sdeck_real_accel;
		bool sdeck_orient_dirty;
		bool vertical_sdeck;
#endif

#if CHIAKI_GUI_ENABLE_SPEEX
		SpeexEchoState *echo_state;
		SpeexPreprocessState *preprocess_state;
		bool speech_processing_enabled;
		uint8_t *echo_resampler_buf, *mic_resampler_buf;
		QQueue<int16_t *> echo_to_cancel;
#endif

		// Cloud streaming parameter storage
		QByteArray cloud_launch_spec_storage;
		QByteArray cloud_handshake_key_storage;
		QByteArray cloud_session_id_storage;
		QByteArray host_storage;

		void PushAudioFrame(int16_t *buf, size_t samples_count);
		void PushHapticsFrame(uint8_t *buf, size_t buf_size);
		void CantDisplayMessage(bool cant_display);
		ChiakiErrorCode InitiatePsnConnection(QString psn_token);
#ifdef Q_OS_MACOS
		void SetMicAuthorization(Authorization authorization);
#endif
#if CHIAKI_GUI_ENABLE_SETSU
		void HandleSetsuEvent(SetsuEvent *event);
#endif
#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
		void HandleSDeckEvent(SDeckEvent *event);
#endif
		void AdjustAdaptiveTriggerPacket(uint8_t *buf, uint8_t type);
		void WaitHaptics();
		void RetryHaptics(quint64 generation);
		void TriggerFfmpegFrameAvailable();

	private slots:
		void InitAudio(unsigned int channels, unsigned int rate);
		void InitMic(unsigned int channels, unsigned int rate);
		void InitHaptics();
		void Event(ChiakiEvent *event);
		void DisconnectHaptics();
		void ConnectHaptics();
#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
		void SdeckQueueHaptics(haptic_packet_t packetl, haptic_packet_t packetr);
		void ConnectSdeckHaptics();
#endif
		void QueueRumbleHaptics(uint16_t strength);
		void ConnectRumbleHaptics();
		void UpdateGamepads();
		void DpadSendFeedbackState();
		void SendFeedbackState() override;
		void OnGameLauncherCompleted();

	public:
		explicit PlayStationStreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent = nullptr);
		~PlayStationStreamSession() override;

		bool IsConnected() override { return connected; }
		bool IsStarted() const override { return session_started; }
		bool IsConnecting() override { return connect_timer.isValid(); }

		void Start() override;
		void Stop() override;
		void GoToBed() override;
		void ToggleMute() override;
		void SetLoginPIN(const QString &pin) override;
		void GoHome() override;

		QString GetResolution() override;
		bool IsCloudStreaming() override { return chiaki_service_type_is_cloud(service_type); }

		ChiakiErrorCode ConnectPsnConnection(QString duid, bool ps5) override;
		void CancelPsnConnection(bool stop_thread) override;

		ChiakiLog *GetChiakiLog() override { return log.GetChiakiLog(); }
		ChiakiSession *GetChiakiSession() override { return &session; }

		void HandleKeyboardEvent(QKeyEvent *event) override;
		void HandleTouchEvent(QTouchEvent *event, qreal width, qreal height) override;
		void HandleDpadTouchEvent(ChiakiControllerState *state, bool placeholder = false) override;
		void HandleMouseReleaseEvent(QMouseEvent *event) override;
		void HandleMousePressEvent(QMouseEvent *event) override;
		void HandleMouseMoveEvent(QMouseEvent *event, qreal width, qreal height) override;
		void ReadMic(const QByteArray &micdata) override;

		StreamBackend GetBackend() const override { return StreamBackend::PlayStation; }
};

#endif // CHIAKI_PLAYSTATIONSTREAMSESSION_H
