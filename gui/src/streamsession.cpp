// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <streamsession.h>
#include <playstationstreamsession.h>
#include <settings.h>
#include <sessionlog.h>

static bool isLocalAddress(QString host)
{
    if(host.contains("."))
    {
        if(host.startsWith("10."))
            return true;
        else if(host.startsWith("192.168."))
            return true;
        for (int j = 16; j < 32; j++)
        {
            if(host.startsWith(QString("172.") + QString::number(j) + QString(".")))
                return true;
        }
    }
    else if(host.contains(":"))
    {
        if(host.startsWith("FC", Qt::CaseInsensitive))
            return true;
        if(host.startsWith("FD", Qt::CaseInsensitive))
            return true;
    }
    return false;
}

StreamSessionConnectInfo::StreamSessionConnectInfo(
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
		bool stretch)
	: settings(settings)
{
	stream_backend = StreamBackend::PlayStation;
	key_map = settings->GetControllerMappingForDecoding();
	decoder = settings->GetDecoder();
	hw_decoder = settings->GetHardwareDecoder();
	hw_device_ctx = nullptr;
	audio_out_device = settings->GetAudioOutDevice();
	audio_in_device = settings->GetAudioInDevice();
	log_level_mask = settings->GetLogLevelMask();
	audio_volume = settings->GetAudioVolume();
	log_file = CreateLogFilename();
	// local connection
	if(duid.isEmpty() && isLocalAddress(host))
		video_profile = chiaki_target_is_ps5(target) ? settings->GetVideoProfileLocalPS5(): settings->GetVideoProfileLocalPS4();
	// remote connection
	else
		video_profile = chiaki_target_is_ps5(target) ? settings->GetVideoProfileRemotePS5(): settings->GetVideoProfileRemotePS4();
	this->target = target;
	this->nickname = std::move(nickname);
	this->host = std::move(host);
	this->regist_key = std::move(regist_key);
	this->morning = std::move(morning);
	this->initial_login_pin = std::move(initial_login_pin);
	audio_buffer_size = settings->GetAudioBufferSize();
	this->fullscreen = fullscreen;
	this->zoom = zoom;
	this->stretch = stretch;
	this->keyboard_controller_enabled = settings->GetKeyboardEnabled();
	this->mouse_touch_enabled = settings->GetMouseTouchEnabled();
	this->enable_keyboard = false;  // Will be set based on game_name when creating session
	this->enable_dualsense = true;
	this->rumble_haptics_intensity = settings->GetRumbleHapticsIntensity();
	this->buttons_by_pos = settings->GetButtonsByPosition();
	this->start_mic_unmuted = settings->GetStartMicUnmuted();
	this->packet_loss_max = settings->GetPacketLossMax();
	this->audio_video_disabled = settings->GetAudioVideoDisabled();
	this->haptic_override = settings->GetHapticOverride();
#if CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE
	this->enable_steamdeck_haptics = settings->GetSteamDeckHapticsEnabled();
	this->vertical_sdeck = settings->GetVerticalDeckEnabled();
#endif
#if CHIAKI_GUI_ENABLE_SPEEX
	this->speech_processing_enabled = settings->GetSpeechProcessingEnabled();
	this->noise_suppress_level = settings->GetNoiseSuppressLevel();
	this->echo_suppress_level = settings->GetEchoSuppressLevel();
#endif
	this->psn_token = settings->GetPsnAuthToken();
	this->psn_account_id = settings->GetPsnAccountId();
	this->duid = std::move(duid);
	this->auto_regist = auto_regist;
	this->dpad_touch_increment = settings->GetDpadTouchEnabled() ? settings->GetDpadTouchIncrement(): 0;
	this->dpad_touch_shortcut1 = settings->GetDpadTouchShortcut1();
	if(this->dpad_touch_shortcut1 > 0)
		this->dpad_touch_shortcut1 = 1 << (this->dpad_touch_shortcut1 - 1);
	this->dpad_touch_shortcut2 = settings->GetDpadTouchShortcut2();
	if(this->dpad_touch_shortcut2 > 0)
		this->dpad_touch_shortcut2 = 1 << (this->dpad_touch_shortcut2 - 1);
	this->dpad_touch_shortcut3 = settings->GetDpadTouchShortcut3();
	if(this->dpad_touch_shortcut3 > 0)
		this->dpad_touch_shortcut3 = 1 << (this->dpad_touch_shortcut3 - 1);
	this->dpad_touch_shortcut4 = settings->GetDpadTouchShortcut4();
	if(this->dpad_touch_shortcut4 > 0)
		this->dpad_touch_shortcut4 = 1 << (this->dpad_touch_shortcut4 - 1);
	this->service_type = CHIAKI_SERVICE_TYPE_REMOTE_PLAY;
	this->cloud_launch_spec = QString();
	this->cloud_handshake_key = QString();
}

StreamSession *StreamSession::Create(const StreamSessionConnectInfo &connect_info, QObject *parent)
{
	if (connect_info.isXbox()) {
		// Will return XboxStreamSession in Phase 3
		return nullptr;
	}
	return new PlayStationStreamSession(connect_info, parent);
}

StreamSession::StreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent)
	: QObject(parent),
	  connected(false),
	  muted(false),
	  mic_connected(false),
	  allow_unmute(true),
	  input_block(0),
	  host(connect_info.host),
	  title_id(connect_info.title_id),
	  fullscreen(connect_info.fullscreen),
	  zoom(connect_info.zoom),
	  stretch(connect_info.stretch),
	  audio_volume(connect_info.audio_volume),
	  cant_display(false),
	  start_mic_unmuted(connect_info.start_mic_unmuted),
	  session_started(false),
	  ffmpeg_decoder(nullptr),
#if CHIAKI_LIB_ENABLE_PI_DECODER
	  pi_decoder(nullptr),
#endif
	  audio_out(0),
	  audio_in(0),
	  audio_out_sample_size(0),
	  audio_out_drain_queue(false),
	  audio_buffer_size(connect_info.audio_buffer_size),
	  mouse_touch_enabled(connect_info.mouse_touch_enabled),
	  keyboard_controller_enabled(connect_info.keyboard_controller_enabled),
	  key_map(connect_info.key_map)
{
	memset(&keyboard_state, 0, sizeof(keyboard_state));
	mic_buf.buf = nullptr;
	mic_buf.size_bytes = 0;
	mic_buf.current_byte = 0;
}

StreamSession::~StreamSession()
{
}
