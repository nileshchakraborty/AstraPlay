// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_STREAMSESSION_H
#define CHIAKI_STREAMSESSION_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QSet>
#include <QImage>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QQueue>
#include <QElapsedTimer>
#include <QPointer>
#include <QHash>

#include <chiaki/common.h>
#include <chiaki/session.h>
#include <chiaki/ffmpegdecoder.h>

#if CHIAKI_LIB_ENABLE_PI_DECODER
#include <chiaki/pidecoder.h>
#endif

#if CHIAKI_GUI_ENABLE_SETSU
#include <setsu.h>
#include <chiaki/orientation.h>
#endif

#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
#include <sdeck.h>
#include <chiaki/orientation.h>
#endif

// Using Q_OS_MACOS instead of __APPLE__ doesn't work for the necessary enums to be included
#ifdef __APPLE__
#include <macMicPermission.h>
#endif

#include "exception.h"
#include "controllermanager.h"
#include "settings.h"

class QKeyEvent;
class Settings;
class GameLauncher;

class ChiakiException: public Exception
{
	public:
		explicit ChiakiException(const QString &msg) : Exception(msg) {};
};

#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
typedef struct haptic_packet_t
{
   	int16_t haptic_packet[30];
    uint64_t timestamp;
} haptic_packet_t;
#endif

enum class StreamBackend
{
	PlayStation,
	Xbox
};

struct StreamSessionConnectInfo
{
	StreamBackend stream_backend = StreamBackend::PlayStation;
	Settings *settings;
	QMap<Qt::Key, int> key_map;
	Decoder decoder;
	QString hw_decoder;
	AVBufferRef *hw_device_ctx;
	QString audio_out_device;
	QString audio_in_device;
	uint32_t log_level_mask;
	QString log_file;
	ChiakiTarget target;
	QString host;
	QString nickname;
	QByteArray regist_key;
	QByteArray morning;
	QString initial_login_pin;
	ChiakiConnectVideoProfile video_profile;
	double packet_loss_max;
	unsigned int audio_buffer_size;
	int audio_volume;
	bool fullscreen;
	bool zoom;
	bool stretch;
	bool enable_keyboard;
	bool keyboard_controller_enabled;
	bool mouse_touch_enabled;
	bool enable_dualsense;
	bool auto_regist;
	float haptic_override;
	ChiakiDisableAudioVideo audio_video_disabled;
	RumbleHapticsIntensity rumble_haptics_intensity;
	bool buttons_by_pos;
	bool start_mic_unmuted;
#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
	bool vertical_sdeck;
	bool enable_steamdeck_haptics;
# endif
#if CHIAKI_GUI_ENABLE_SPEEX
	bool speech_processing_enabled;
	int32_t noise_suppress_level;
	int32_t echo_suppress_level;
#endif
	QString duid;
	QString psn_token;
	QString psn_account_id;
	uint16_t dpad_touch_increment;
	uint dpad_touch_shortcut1;
	uint dpad_touch_shortcut2;
	uint dpad_touch_shortcut3;
	uint dpad_touch_shortcut4;
	QString game_name;
	QString title_id;
	// Streaming service type (REMOTE_PLAY/PSNOW/PSCLOUD)
	ChiakiServiceType service_type;
	QString cloud_launch_spec;
	QString cloud_handshake_key;
	QString cloud_session_id;
	uint8_t cloud_psn_wrapper_type;
	uint32_t cloud_mtu_in; // MTU in from ping results (0 if not set)
	uint32_t cloud_mtu_out; // MTU out from ping results (0 if not set)
	uint64_t cloud_rtt_us; // RTT in microseconds from ping results (0 if not set)

	bool isXbox() const { return stream_backend == StreamBackend::Xbox; }
	bool isPlayStation() const { return stream_backend == StreamBackend::PlayStation; }

	StreamSessionConnectInfo() {}
	StreamSessionConnectInfo(
			Settings *settings,
			ChiakiTarget target,
			QString host,
			QString nickname,
			QByteArray regist_key,
			QByteArray morning,
			QString initial_login_pin,
			QString duid,
			bool auto_regist,
			bool fullscreen,
			bool zoom,
			bool stretch);
};

struct MicBuf
{
	int16_t *buf;
	uint32_t size_bytes;
	uint32_t current_byte;
};

class StreamSession : public QObject
{
	friend class StreamSessionPrivate;
	friend class GameLauncher;

	Q_OBJECT
	Q_PROPERTY(QString host READ GetHost CONSTANT)
	Q_PROPERTY(QString titleId READ GetTitleId CONSTANT)
	Q_PROPERTY(bool connected READ GetConnected NOTIFY ConnectedChanged)
	Q_PROPERTY(double measuredBitrate READ GetMeasuredBitrate NOTIFY MeasuredBitrateChanged)
	Q_PROPERTY(double averagePacketLoss READ GetAveragePacketLoss NOTIFY AveragePacketLossChanged)
	Q_PROPERTY(double measuredFps READ GetMeasuredFps NOTIFY MeasuredFpsChanged)
	Q_PROPERTY(double measuredRtt READ GetMeasuredRtt NOTIFY MeasuredRttChanged)
	Q_PROPERTY(QString resolution READ GetResolution NOTIFY ResolutionChanged)
	Q_PROPERTY(bool muted READ GetMuted WRITE SetMuted NOTIFY MutedChanged)
	Q_PROPERTY(bool cantDisplay READ GetCantDisplay NOTIFY CantDisplayChanged)
	Q_PROPERTY(QString loadingMessage READ GetLoadingMessage WRITE SetLoadingMessage NOTIFY LoadingMessageChanged)
	Q_PROPERTY(bool isCloudStreaming READ IsCloudStreaming CONSTANT)
	Q_PROPERTY(bool isXbox READ IsXbox CONSTANT)
	Q_PROPERTY(bool fullscreen READ GetFullscreen CONSTANT)
	Q_PROPERTY(bool zoom READ GetZoom CONSTANT)
	Q_PROPERTY(bool stretch READ GetStretch CONSTANT)
	Q_PROPERTY(QString controllerBattery READ GetControllerBattery NOTIFY ControllerBatteryChanged)

	protected:
		bool connected;
		bool muted;
		bool mic_connected;
#ifdef Q_OS_MACOS
		bool mic_authorization;
#endif
		bool allow_unmute;
		int input_block;
		QString host;
		QString title_id;
		bool fullscreen;
		bool zoom;
		bool stretch;
		int audio_volume;
		double measured_bitrate = 0;
		double average_packet_loss = 0;
		double measured_fps = 0;
		double measured_rtt = 0;
		QString resolution_str;
		QList<double> packet_loss_history;
		bool cant_display = false;
		QString loading_message;
		QString controller_battery;
		QHash<int, Controller *> controllers;
		bool start_mic_unmuted;
		bool session_started;
		QPointer<GameLauncher> game_launcher;

		ChiakiControllerState keyboard_state;

		ChiakiFfmpegDecoder *ffmpeg_decoder;
#if CHIAKI_LIB_ENABLE_PI_DECODER
		ChiakiPiDecoder *pi_decoder;
#endif

		QString audio_out_device_name;
		QString audio_in_device_name;
		SDL_AudioDeviceID audio_out;
		SDL_AudioDeviceID audio_in;
		size_t audio_out_sample_size;
		bool audio_out_drain_queue;
		unsigned int audio_buffer_size;
		bool mouse_touch_enabled;
		bool keyboard_controller_enabled;
		MicBuf mic_buf;
		QMap<Qt::Key, int> key_map;
		QElapsedTimer connect_timer;

	public:
		static StreamSession *Create(const StreamSessionConnectInfo &connect_info, QObject *parent = nullptr);

		explicit StreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent = nullptr);
		~StreamSession() override;

		virtual bool IsConnected() { return connected; }
		virtual bool IsStarted() const { return session_started; }
		virtual bool IsConnecting() { return connect_timer.isValid(); }

		virtual void Start() = 0;
		virtual void Stop() = 0;
		virtual void GoToBed() = 0;
		virtual void ToggleMute() {}
		virtual void SetLoginPIN(const QString &pin) { Q_UNUSED(pin); }
		virtual void GoHome() = 0;

		QString GetHost() { return host; }
		QString GetTitleId() { return title_id; }
		bool GetConnected() { return connected; }
		double GetMeasuredBitrate()	{ return measured_bitrate; }
		double GetAveragePacketLoss()	{ return average_packet_loss; }
		double GetMeasuredFps()	{ return measured_fps; }
		double GetMeasuredRtt()	{ return measured_rtt; }
		virtual QString GetResolution() = 0;
		bool GetMuted()	{ return muted; }
		void SetMuted(bool enable)	{ if (enable != muted) ToggleMute(); }
		Q_INVOKABLE int GetAudioVolume() { return audio_volume; }
		Q_INVOKABLE void SetAudioVolume(int volume) { audio_volume = volume; }
		bool GetCantDisplay() const	{ return cant_display; }
		QString GetLoadingMessage() { return loading_message; }
		void SetLoadingMessage(const QString &message) { 
			if (loading_message != message) {
				loading_message = message;
				emit LoadingMessageChanged();
			}
		}
		virtual bool IsCloudStreaming() = 0;
		virtual bool IsXbox() const { return GetBackend() == StreamBackend::Xbox; }
		bool GetFullscreen() { return fullscreen; }
		bool GetZoom() { return zoom; }
		bool GetStretch() { return stretch; }
		QString GetControllerBattery() { return controller_battery; }

		virtual ChiakiErrorCode ConnectPsnConnection(QString duid, bool ps5) { Q_UNUSED(duid); Q_UNUSED(ps5); return CHIAKI_ERR_UNKNOWN; }
		virtual void CancelPsnConnection(bool stop_thread) { Q_UNUSED(stop_thread); }

		virtual ChiakiLog *GetChiakiLog() { return nullptr; }
		virtual ChiakiSession *GetChiakiSession() { return nullptr; }
		QList<Controller *> GetControllers() { return controllers.values(); }
		ChiakiFfmpegDecoder *GetFfmpegDecoder() { return ffmpeg_decoder; }
#if CHIAKI_LIB_ENABLE_PI_DECODER
		ChiakiPiDecoder *GetPiDecoder() { return pi_decoder; }
#endif

		virtual void HandleKeyboardEvent(QKeyEvent *event) { Q_UNUSED(event); }
		virtual void HandleTouchEvent(QTouchEvent *event, qreal width, qreal height) { Q_UNUSED(event); Q_UNUSED(width); Q_UNUSED(height); }
		virtual void HandleDpadTouchEvent(ChiakiControllerState *state, bool placeholder = false) { Q_UNUSED(state); Q_UNUSED(placeholder); }
		virtual void HandleMouseReleaseEvent(QMouseEvent *event) { Q_UNUSED(event); }
		virtual void HandleMousePressEvent(QMouseEvent *event) { Q_UNUSED(event); }
		virtual void HandleMouseMoveEvent(QMouseEvent *event, qreal width, qreal height) { Q_UNUSED(event); Q_UNUSED(width); Q_UNUSED(height); }
		virtual void ReadMic(const QByteArray &micdata) { Q_UNUSED(micdata); }

		void BlockInput(bool block) { input_block = block ? 1 : 2; SendFeedbackState(); }
		virtual void SendFeedbackState() = 0;

		virtual StreamBackend GetBackend() const = 0;

	signals:
		void FfmpegFrameAvailable();
		void RumbleHapticPushed(uint16_t strength);
#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
		void SdeckHapticPushed(haptic_packet_t packetl, haptic_packet_t packetr);
#endif
		void DualSenseIntensityChanged(uint8_t intensity);
		void SessionQuit(ChiakiQuitReason reason, const QString &reason_str);
		void LoginPINRequested(bool incorrect);
		void DataHolepunchProgress(bool finished);
		void AutoRegistSucceeded(const ChiakiRegisteredHost &host);
		void NicknameReceived(QString nickname);
		void ConnectedChanged();
		void MeasuredBitrateChanged();
		void AveragePacketLossChanged();
		void MeasuredFpsChanged();
		void MeasuredRttChanged();
		void ResolutionChanged();
		void MutedChanged();
		void CantDisplayChanged(bool cant_display);
		void LoadingMessageChanged();
		void ControllerBatteryChanged();
		void GameLaunchCompleted();
		void PsChordFired();
};

Q_DECLARE_METATYPE(ChiakiQuitReason)
#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
Q_DECLARE_METATYPE(haptic_packet_t)
#endif

#endif // CHIAKI_STREAMSESSION_H
