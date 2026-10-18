// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/congestioncontrol.h>

#define CONGESTION_CONTROL_INTERVAL_MS 200

static void *congestion_control_thread_func(void *user)
{
	ChiakiCongestionControl *control = user;

	ChiakiErrorCode err = chiaki_bool_pred_cond_lock(&control->stop_cond);
	if(err != CHIAKI_ERR_SUCCESS)
		return NULL;

	while(true)
	{
		err = chiaki_bool_pred_cond_timedwait(&control->stop_cond, CONGESTION_CONTROL_INTERVAL_MS);
		if(err != CHIAKI_ERR_TIMEOUT)
			break;

		uint64_t received;
		uint64_t lost;
		chiaki_packet_stats_get(control->stats, true, &received, &lost);
		ChiakiTakionCongestionPacket packet = { 0 };
		uint64_t total = received + lost;
		control->packet_loss = total > 0 ? (double)lost / total : 0;

		/* Dynamic bitrate and FEC congestion heuristics:
		 * If sustained packet loss occurs, we incrementally scale down dynamic_scale
		 * and signal the console's encoder via adjusted packet loss feedback.
		 * When loss clears, we conservatively ramp back up (AIMD - additive increase,
		 * multiplicative decrease) to restore peak fidelity without causing buffer bloat.
		 */
		if(control->packet_loss > 0.02) /* > 2% packet loss */
		{
			control->congestion_ticks++;
			control->recovery_ticks = 0;
			/* Scale down rate by ~10% per congestion tick, floor at 35% of profile */
			if(control->dynamic_scale > 0.35)
			{
				control->dynamic_scale -= 0.10;
				if(control->dynamic_scale < 0.35)
					control->dynamic_scale = 0.35;
				CHIAKI_LOGI(control->takion->log, "Congestion heuristic: packet loss %.2f%%, throttling scale to %.2f",
					control->packet_loss * 100.0, control->dynamic_scale);
			}
		}
		else if(total > 0 && control->packet_loss == 0.0)
		{
			control->recovery_ticks++;
			control->congestion_ticks = 0;
			/* After 5 consecutive clean ticks (~1.0s), cautiously step up (+5%) */
			if(control->recovery_ticks >= 5 && control->dynamic_scale < 1.0)
			{
				control->dynamic_scale += 0.05;
				if(control->dynamic_scale > 1.0)
					control->dynamic_scale = 1.0;
				control->recovery_ticks = 0;
				CHIAKI_LOGD(control->takion->log, "Recovery heuristic: link stable, scaling up to %.2f", control->dynamic_scale);
			}
		}

		if(control->packet_loss_max > 0.0 && control->packet_loss > control->packet_loss_max)
		{
			/* If congestion persists (> 5 ticks / ~1.0s), allow genuine loss feedback to reach the console
			 * so the encoder lowers its QP and bitrate rather than overwhelming the network buffer */
			if(control->congestion_ticks > 5)
			{
				CHIAKI_LOGD(control->takion->log, "Sustained congestion (%u ticks): reporting un-clamped loss %.2f%% to console encoder",
					(unsigned int)control->congestion_ticks, control->packet_loss * 100.0);
			}
			else
			{
				CHIAKI_LOGW(control->takion->log, "Transient burst loss: clamping loss to %.2f%% to prevent premature stream degradation",
					control->packet_loss_max * 100.0);
				lost = (uint64_t)(total * control->packet_loss_max);
				received = total - lost;
			}
		}

		packet.received = (uint16_t)received;
		packet.lost = (uint16_t)lost;
		CHIAKI_LOGV(control->takion->log, "Sending Congestion Control Packet, received: %u, lost: %u (scale: %.2f)",
			(unsigned int)packet.received, (unsigned int)packet.lost, control->dynamic_scale);
		chiaki_takion_send_congestion(control->takion, &packet);
	}

	chiaki_bool_pred_cond_unlock(&control->stop_cond);
	return NULL;
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_congestion_control_start(ChiakiCongestionControl *control, ChiakiTakion *takion, ChiakiPacketStats *stats, double packet_loss_max)
{
	control->takion = takion;
	control->stats = stats;
	control->packet_loss_max = packet_loss_max;
	control->packet_loss = 0;
	control->dynamic_scale = 1.0;
	control->congestion_ticks = 0;
	control->recovery_ticks = 0;

	ChiakiErrorCode err = chiaki_bool_pred_cond_init(&control->stop_cond);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;

	err = chiaki_thread_create(&control->thread, congestion_control_thread_func, control);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		chiaki_bool_pred_cond_fini(&control->stop_cond);
		return err;
	}

	chiaki_thread_set_name(&control->thread, "Chiaki Congestion Control");

	return CHIAKI_ERR_SUCCESS;
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_congestion_control_stop(ChiakiCongestionControl *control)
{
	ChiakiErrorCode err = chiaki_bool_pred_cond_signal(&control->stop_cond);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;

	err = chiaki_thread_join(&control->thread, NULL);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;
	control->thread.thread = 0;

	return chiaki_bool_pred_cond_fini(&control->stop_cond);
}
