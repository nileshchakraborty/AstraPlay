// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>
#include <chiaki/controller.h>

static MunitResult test_stick_calibration_radial_deadzone(const MunitParameter params[], void *user)
{
	(void)params;
	(void)user;

	ChiakiStickCalibration calib;
	chiaki_stick_calibration_init_default(&calib);
	calib.deadzone = 0.10f; // 10% deadzone
	calib.mode = CHIAKI_DEADZONE_RADIAL;

	/* 1. Value inside deadzone (e.g. 5% drift = 1638) must be clamped to 0 */
	int16_t x = 1638;
	int16_t y = 0;
	chiaki_stick_apply_calibration(&x, &y, &calib);
	munit_assert_int16(x, ==, 0);
	munit_assert_int16(y, ==, 0);

	/* 2. Diagonal drift inside deadzone: r = sqrt(0.05^2 + 0.05^2) = ~0.07 <= 0.10 */
	x = 1638;
	y = 1638;
	chiaki_stick_apply_calibration(&x, &y, &calib);
	munit_assert_int16(x, ==, 0);
	munit_assert_int16(y, ==, 0);

	/* 3. Value outside deadzone (e.g. 100% full tilt = 32767) must reach full tilt */
	x = 32767;
	y = 0;
	chiaki_stick_apply_calibration(&x, &y, &calib);
	munit_assert_int16(x, ==, 32767);
	munit_assert_int16(y, ==, 0);

	/* 4. Full tilt negative */
	x = -32767;
	y = 0;
	chiaki_stick_apply_calibration(&x, &y, &calib);
	munit_assert_int16(x, ==, -32767);
	munit_assert_int16(y, ==, 0);

	return MUNIT_OK;
}

static MunitResult test_stick_calibration_axial_deadzone(const MunitParameter params[], void *user)
{
	(void)params;
	(void)user;

	ChiakiStickCalibration calib;
	chiaki_stick_calibration_init_default(&calib);
	calib.deadzone = 0.10f; // 10%
	calib.mode = CHIAKI_DEADZONE_AXIAL;

	/* X axis drifting (5%), Y axis intentional movement (50%) */
	int16_t x = 1638; // ~5%
	int16_t y = 16384; // ~50%
	chiaki_stick_apply_calibration(&x, &y, &calib);
	munit_assert_int16(x, ==, 0);
	munit_assert_int16(y, >, 0);

	return MUNIT_OK;
}

static MunitResult test_button_remap_profiles(const MunitParameter params[], void *user)
{
	(void)params;
	(void)user;

	ChiakiControllerState state;
	chiaki_controller_state_set_idle(&state);
	state.buttons = CHIAKI_CONTROLLER_BUTTON_CROSS | CHIAKI_CONTROLLER_BUTTON_L1;
	state.l2_state = 0;

	ChiakiButtonRemap remap;
	chiaki_button_remap_init_default(&remap);
	remap.swap_cross_moon = true;
	remap.swap_l1_l2 = true;

	chiaki_controller_state_apply_remap(&state, &remap);

	/* Cross should have swapped to Moon */
	munit_assert_uint32(state.buttons & CHIAKI_CONTROLLER_BUTTON_CROSS, ==, 0);
	munit_assert_uint32(state.buttons & CHIAKI_CONTROLLER_BUTTON_MOON, !=, 0);

	/* L1 should have swapped to L2 (analog trigger = 255) */
	munit_assert_uint32(state.buttons & CHIAKI_CONTROLLER_BUTTON_L1, ==, 0);
	munit_assert_uint8(state.l2_state, ==, 255);

	/* Now test swapping L2 back to L1 */
	chiaki_controller_state_set_idle(&state);
	state.l2_state = 255;
	chiaki_controller_state_apply_remap(&state, &remap);
	munit_assert_uint8(state.l2_state, ==, 0);
	munit_assert_uint32(state.buttons & CHIAKI_CONTROLLER_BUTTON_L1, !=, 0);

	return MUNIT_OK;
}

MunitTest tests_controller[] = {
	{
		"/radial_deadzone",
		test_stick_calibration_radial_deadzone,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/axial_deadzone",
		test_stick_calibration_axial_deadzone,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/button_remap",
		test_button_remap_profiles,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
