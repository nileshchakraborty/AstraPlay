// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/controller.h>
#include <math.h>

#define TOUCH_ID_MASK 0x7f

CHIAKI_EXPORT void chiaki_controller_state_set_idle(ChiakiControllerState *state)
{
	state->buttons = 0;
	state->l2_state = 0;
	state->r2_state = 0;
	state->left_x = 0;
	state->left_y = 0;
	state->right_x = 0;
	state->right_y = 0;
	state->touch_id_next = 0;
	for(size_t i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
	{
		state->touches[i].id = -1;
		state->touches[i].x = 0;
		state->touches[i].y = 0;
	}
	state->gyro_x = state->gyro_y = state->gyro_z = 0.0f;
	state->accel_x = 0.0f;
	state->accel_y = 1.0f;
	state->accel_z = 0.0f;
	state->orient_x = 0.0f;
	state->orient_y = 0.0f;
	state->orient_z = 0.0f;
	state->orient_w = 1.0f;
}

CHIAKI_EXPORT int8_t chiaki_controller_state_start_touch(ChiakiControllerState *state, uint16_t x, uint16_t y)
{
	for(size_t i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
	{
		if(state->touches[i].id < 0)
		{
			state->touches[i].id = state->touch_id_next;
			state->touch_id_next = (state->touch_id_next + 1) & TOUCH_ID_MASK;
			state->touches[i].x = x;
			state->touches[i].y = y;
			return state->touches[i].id;
		}
	}
	return -1;
}

CHIAKI_EXPORT void chiaki_controller_state_stop_touch(ChiakiControllerState *state, uint8_t id)
{
	for(size_t i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
	{
		if(state->touches[i].id == id)
		{
			state->touches[i].id = -1;
			break;
		}
	}
}

CHIAKI_EXPORT void chiaki_controller_state_set_touch_pos(ChiakiControllerState *state, uint8_t id, uint16_t x, uint16_t y)
{
	id &= TOUCH_ID_MASK;
	for(size_t i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
	{
		if(state->touches[i].id == id)
		{
			state->touches[i].x = x;
			state->touches[i].y = y;
			break;
		}
	}
}

CHIAKI_EXPORT bool chiaki_controller_state_equals(ChiakiControllerState *a, ChiakiControllerState *b)
{
	if(!(a->buttons == b->buttons
		&& a->l2_state == b->l2_state
		&& a->r2_state == b->r2_state
		&& a->left_x == b->left_x
		&& a->left_y == b->left_y
		&& a->right_x == b->right_x
		&& a->right_y == b->right_y))
		return false;

	for(size_t i=0; i<CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
	{
		if(a->touches[i].id != b->touches[i].id)
			return false;
		if(a->touches[i].id >= 0 && (a->touches[i].x != b->touches[i].x || a->touches[i].y != b->touches[i].y))
			return false;
	}

#define CHECKF(n) if(a->n < b->n - 0.0000001f || a->n > b->n + 0.0000001f) return false
	CHECKF(gyro_x);
	CHECKF(gyro_y);
	CHECKF(gyro_z);
	CHECKF(accel_x);
	CHECKF(accel_y);
	CHECKF(accel_z);
	CHECKF(orient_x);
	CHECKF(orient_y);
	CHECKF(orient_z);
	CHECKF(orient_w);
#undef CHECKF

	return true;
}

#define MAX(a, b)	  ((a) > (b) ? (a) : (b))
#define ABS(a)		  ((a) > 0 ? (a) : -(a))
#define MAX_ABS(a, b) (ABS(a) > ABS(b) ? (a) : (b))

CHIAKI_EXPORT void chiaki_controller_state_or(ChiakiControllerState *out, ChiakiControllerState *a, ChiakiControllerState *b)
{
	out->buttons = a->buttons | b->buttons;
	out->l2_state = MAX(a->l2_state, b->l2_state);
	out->r2_state = MAX(a->r2_state, b->r2_state);
	out->left_x = MAX_ABS(a->left_x, b->left_x);
	out->left_y = MAX_ABS(a->left_y, b->left_y);
	out->right_x = MAX_ABS(a->right_x, b->right_x);
	out->right_y = MAX_ABS(a->right_y, b->right_y);

	out->touch_id_next = 0;
	for(size_t i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
	{
		ChiakiControllerTouch *touch = a->touches[i].id >= 0 ? &a->touches[i] : (b->touches[i].id >= 0 ? &b->touches[i] : NULL);
		if(!touch)
		{
			out->touches[i].id = -1;
			out->touches[i].x = out->touches[i].y = 0;
			continue;
		}
		if(touch != &out->touches[i])
			out->touches[i] = *touch;
	}
	// If first value has gyro set take it, otherwise give 2nd value
	// prioritizes DualSense over Steam Deck for instance if it's attached 
	// since setsu and controller_state are before steamdeck
	// don't mix gryo / accel values from different controllers with simple or
	// doing so will cause problems w/ orientation values among other things
	// since orient is calculated from accel and gyro, just check those.
	bool chooser = false;
#define SETCHOOSER(n, default) if(a->n < (default - 0.0000001f) ||  a->n > (default + 0.0000001f)) chooser = true;
	SETCHOOSER(gyro_x, 0.0f);
	SETCHOOSER(gyro_y, 0.0f);
	SETCHOOSER(gyro_z, 0.0f);
	SETCHOOSER(accel_x, 0.0f);
	SETCHOOSER(accel_y, 1.0f);
	SETCHOOSER(accel_z, 0.0f);
#undef SETCHOOSER
	 
#define CHOOSEF(n, chooser) if(chooser == true) out->n = a->n; else out->n = b->n;
	CHOOSEF(gyro_x, chooser);
	CHOOSEF(gyro_y, chooser);
	CHOOSEF(gyro_z, chooser);
	CHOOSEF(accel_x, chooser);
	CHOOSEF(accel_y, chooser);
	CHOOSEF(accel_z, chooser);
	CHOOSEF(orient_x, chooser);
	CHOOSEF(orient_y, chooser);
	CHOOSEF(orient_z, chooser);
	CHOOSEF(orient_w, chooser);
#undef CHOOSEF
}

CHIAKI_EXPORT void chiaki_stick_calibration_init_default(ChiakiStickCalibration *calib)
{
	if (!calib)
		return;
	calib->deadzone = 0.0f;
	calib->sensitivity = 1.0f;
	calib->linearity = 1.0f;
	calib->mode = CHIAKI_DEADZONE_RADIAL;
}

CHIAKI_EXPORT void chiaki_stick_apply_calibration(int16_t *x, int16_t *y, const ChiakiStickCalibration *calib)
{
	if (!x || !y || !calib)
		return;

	float deadzone = calib->deadzone;
	if (deadzone < 0.0f)
		deadzone = 0.0f;
	else if (deadzone > 0.9f)
		deadzone = 0.9f;

	float sensitivity = calib->sensitivity;
	if (sensitivity <= 0.0f)
		sensitivity = 1.0f;

	float linearity = calib->linearity;
	if (linearity <= 0.0f)
		linearity = 1.0f;

	float nx = (float)*x / 32767.0f;
	float ny = (float)*y / 32767.0f;

	if (calib->mode == CHIAKI_DEADZONE_RADIAL)
	{
		float r = sqrtf(nx * nx + ny * ny);
		if (r <= deadzone || r < 1e-5f)
		{
			*x = 0;
			*y = 0;
			return;
		}

		/* Remap smoothly between [deadzone, 1.0] to [0.0, 1.0] */
		float scaled_r = (r - deadzone) / (1.0f - deadzone);
		if (scaled_r > 1.0f)
			scaled_r = 1.0f;

		if (linearity != 1.0f)
			scaled_r = powf(scaled_r, linearity);

		scaled_r *= sensitivity;
		if (scaled_r > 1.0f)
			scaled_r = 1.0f;

		float ux = nx / r;
		float uy = ny / r;

		float out_x = ux * scaled_r * 32767.0f;
		float out_y = uy * scaled_r * 32767.0f;

		if (out_x > 32767.0f) out_x = 32767.0f;
		else if (out_x < -32768.0f) out_x = -32768.0f;

		if (out_y > 32767.0f) out_y = 32767.0f;
		else if (out_y < -32768.0f) out_y = -32768.0f;

		*x = (int16_t)out_x;
		*y = (int16_t)out_y;
	}
	else
	{
		float abs_x = fabsf(nx);
		float abs_y = fabsf(ny);

		float out_x = 0.0f;
		if (abs_x > deadzone)
		{
			float scaled_x = (abs_x - deadzone) / (1.0f - deadzone);
			if (scaled_x > 1.0f) scaled_x = 1.0f;
			if (linearity != 1.0f) scaled_x = powf(scaled_x, linearity);
			scaled_x *= sensitivity;
			if (scaled_x > 1.0f) scaled_x = 1.0f;
			out_x = (nx > 0.0f ? scaled_x : -scaled_x) * 32767.0f;
		}

		float out_y = 0.0f;
		if (abs_y > deadzone)
		{
			float scaled_y = (abs_y - deadzone) / (1.0f - deadzone);
			if (scaled_y > 1.0f) scaled_y = 1.0f;
			if (linearity != 1.0f) scaled_y = powf(scaled_y, linearity);
			scaled_y *= sensitivity;
			if (scaled_y > 1.0f) scaled_y = 1.0f;
			out_y = (ny > 0.0f ? scaled_y : -scaled_y) * 32767.0f;
		}

		if (out_x > 32767.0f) out_x = 32767.0f;
		else if (out_x < -32768.0f) out_x = -32768.0f;

		if (out_y > 32767.0f) out_y = 32767.0f;
		else if (out_y < -32768.0f) out_y = -32768.0f;

		*x = (int16_t)out_x;
		*y = (int16_t)out_y;
	}
}

CHIAKI_EXPORT void chiaki_controller_state_apply_calibration(ChiakiControllerState *state,
                                                             const ChiakiStickCalibration *left,
                                                             const ChiakiStickCalibration *right)
{
	if (!state)
		return;
	if (left)
		chiaki_stick_apply_calibration(&state->left_x, &state->left_y, left);
	if (right)
		chiaki_stick_apply_calibration(&state->right_x, &state->right_y, right);
}

CHIAKI_EXPORT void chiaki_button_remap_init_default(ChiakiButtonRemap *remap)
{
	if (!remap)
		return;
	remap->swap_cross_moon = false;
	remap->swap_box_pyramid = false;
	remap->swap_l1_l2 = false;
	remap->swap_r1_r2 = false;
	remap->swap_l3_r3 = false;
}

CHIAKI_EXPORT void chiaki_controller_state_apply_remap(ChiakiControllerState *state, const ChiakiButtonRemap *remap)
{
	if (!state || !remap)
		return;

	if (remap->swap_cross_moon)
	{
		uint32_t cross = state->buttons & CHIAKI_CONTROLLER_BUTTON_CROSS;
		uint32_t moon = state->buttons & CHIAKI_CONTROLLER_BUTTON_MOON;
		state->buttons &= ~(CHIAKI_CONTROLLER_BUTTON_CROSS | CHIAKI_CONTROLLER_BUTTON_MOON);
		if (cross) state->buttons |= CHIAKI_CONTROLLER_BUTTON_MOON;
		if (moon) state->buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS;
	}

	if (remap->swap_box_pyramid)
	{
		uint32_t box = state->buttons & CHIAKI_CONTROLLER_BUTTON_BOX;
		uint32_t pyr = state->buttons & CHIAKI_CONTROLLER_BUTTON_PYRAMID;
		state->buttons &= ~(CHIAKI_CONTROLLER_BUTTON_BOX | CHIAKI_CONTROLLER_BUTTON_PYRAMID);
		if (box) state->buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
		if (pyr) state->buttons |= CHIAKI_CONTROLLER_BUTTON_BOX;
	}

	if (remap->swap_l3_r3)
	{
		uint32_t l3 = state->buttons & CHIAKI_CONTROLLER_BUTTON_L3;
		uint32_t r3 = state->buttons & CHIAKI_CONTROLLER_BUTTON_R3;
		state->buttons &= ~(CHIAKI_CONTROLLER_BUTTON_L3 | CHIAKI_CONTROLLER_BUTTON_R3);
		if (l3) state->buttons |= CHIAKI_CONTROLLER_BUTTON_R3;
		if (r3) state->buttons |= CHIAKI_CONTROLLER_BUTTON_L3;
	}

	if (remap->swap_l1_l2)
	{
		bool l1_pressed = (state->buttons & CHIAKI_CONTROLLER_BUTTON_L1) != 0;
		uint8_t l2_val = state->l2_state;

		state->buttons &= ~CHIAKI_CONTROLLER_BUTTON_L1;
		state->l2_state = l1_pressed ? 255 : 0;
		if (l2_val > 0)
			state->buttons |= CHIAKI_CONTROLLER_BUTTON_L1;
	}

	if (remap->swap_r1_r2)
	{
		bool r1_pressed = (state->buttons & CHIAKI_CONTROLLER_BUTTON_R1) != 0;
		uint8_t r2_val = state->r2_state;

		state->buttons &= ~CHIAKI_CONTROLLER_BUTTON_R1;
		state->r2_state = r1_pressed ? 255 : 0;
		if (r2_val > 0)
			state->buttons |= CHIAKI_CONTROLLER_BUTTON_R1;
	}
}

