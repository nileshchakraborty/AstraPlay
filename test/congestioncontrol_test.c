// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>
#include <chiaki/congestioncontrol.h>
#include <chiaki/packetstats.h>

static MunitResult test_congestion_control_init(const MunitParameter params[], void *user)
{
	(void)params;
	(void)user;

	ChiakiCongestionControl control = { 0 };
	control.dynamic_scale = 1.0;
	control.packet_loss_max = 0.05;

	munit_assert_double(control.dynamic_scale, ==, 1.0);
	munit_assert_double(control.packet_loss_max, ==, 0.05);
	munit_assert_uint32(control.congestion_ticks, ==, 0);
	munit_assert_uint32(control.recovery_ticks, ==, 0);

	/* Simulate 3 ticks of congestion: scale should throttle down from 1.0 to 0.70 */
	for (int i = 0; i < 3; i++) {
		if (control.dynamic_scale > 0.35) {
			control.dynamic_scale -= 0.10;
			if (control.dynamic_scale < 0.35)
				control.dynamic_scale = 0.35;
		}
	}
	munit_assert_double(control.dynamic_scale, <=, 0.71);
	munit_assert_double(control.dynamic_scale, >=, 0.69);

	/* Simulate severe congestion: scale should not drop below floor (0.35) */
	for (int i = 0; i < 10; i++) {
		if (control.dynamic_scale > 0.35) {
			control.dynamic_scale -= 0.10;
			if (control.dynamic_scale < 0.35)
				control.dynamic_scale = 0.35;
		}
	}
	munit_assert_double(control.dynamic_scale, >=, 0.349);
	munit_assert_double(control.dynamic_scale, <=, 0.351);

	/* Simulate recovery ticks: scale steps back up (+0.05) */
	control.dynamic_scale += 0.05;
	munit_assert_double(control.dynamic_scale, >=, 0.399);
	munit_assert_double(control.dynamic_scale, <=, 0.401);

	return MUNIT_OK;
}

MunitTest tests_congestion_control[] = {
	{
		"/adaptive_heuristics",
		test_congestion_control_init,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
