// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.common.ext

import android.app.UiModeManager
import android.content.Context
import android.content.res.Configuration
import android.graphics.drawable.ColorDrawable
import android.util.TypedValue
import android.view.KeyEvent
import android.widget.AdapterView
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import com.google.android.material.dialog.MaterialAlertDialogBuilder

private const val TV_TITLE_SP = 28f
private const val TV_BODY_SP = 24f
private const val TV_BUTTON_SP = 20f
private const val TV_FOCUS_COLOR = 0x44FFD700.toInt()

fun Context.isTv(): Boolean
{
	val uiModeManager = getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
	return uiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION
}

fun Context.hasGamepadOrHandheld(): Boolean
{
	if (isTv()) return true
	try {
		for (id in android.view.InputDevice.getDeviceIds()) {
			val dev = android.view.InputDevice.getDevice(id) ?: continue
			val sources = dev.sources
			if ((sources and android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD ||
				(sources and android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK ||
				(sources and android.view.InputDevice.SOURCE_DPAD) == android.view.InputDevice.SOURCE_DPAD) {
				return true
			}
		}
	} catch (_: Throwable) {}
	return false
}

/**
 * App-wide dialog builder. On phone/tablet it behaves identically to
 * [MaterialAlertDialogBuilder]. On TV it automatically:
 *
 * - Scales title, message and button text for couch-distance readability.
 * - Adds a visible blue overlay on focused buttons for D-pad navigation.
 * - Pre-highlights the positive button and intercepts the first
 *   DPAD_CENTER / ENTER so it activates with a single press even when
 *   the system is in touch mode (common on TV emulators).
 */
class AppAlertDialogBuilder(context: Context) : MaterialAlertDialogBuilder(context)
{
	override fun show(): AlertDialog
	{
		val dialog = super.show()
		if (!context.hasGamepadOrHandheld()) return dialog

		if (context.isTv()) {
			dialog.findViewById<TextView>(android.R.id.message)
				?.setTextSize(TypedValue.COMPLEX_UNIT_SP, TV_BODY_SP)
			dialog.window?.decorView?.findViewById<TextView>(
				androidx.appcompat.R.id.alertTitle
			)?.setTextSize(TypedValue.COMPLEX_UNIT_SP, TV_TITLE_SP)
		}

		val positiveBtn = dialog.getButton(AlertDialog.BUTTON_POSITIVE)

		for (which in intArrayOf(AlertDialog.BUTTON_POSITIVE, AlertDialog.BUTTON_NEGATIVE, AlertDialog.BUTTON_NEUTRAL)) {
			dialog.getButton(which)?.let { btn ->
				if (context.isTv()) {
					btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, TV_BUTTON_SP)
				}
				btn.isFocusable = true
				btn.isFocusableInTouchMode = true
				btn.setOnFocusChangeListener { v, hasFocus ->
					v.foreground = if (hasFocus) ColorDrawable(TV_FOCUS_COLOR) else null
					if (hasFocus && v != positiveBtn) positiveBtn?.foreground = null
				}
			}
		}

		dialog.listView?.let { lv ->
			lv.isFocusable = true
			lv.isFocusableInTouchMode = true
			lv.itemsCanFocus = true
		}

		if (dialog.listView != null && positiveBtn == null) {
			dialog.listView?.requestFocus()
		} else {
			positiveBtn?.foreground = ColorDrawable(TV_FOCUS_COLOR)
			positiveBtn?.requestFocusFromTouch()
		}

		dialog.setOnKeyListener { _, keyCode, event ->
			if (event.action == KeyEvent.ACTION_DOWN) {
				when (keyCode) {
					KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> {
						val focused = dialog.currentFocus
						if (focused != null && focused.isClickable) {
							focused.performClick()
							true
						} else if (dialog.listView != null && dialog.listView.hasFocus()) {
							val pos = dialog.listView.selectedItemPosition
							if (pos != AdapterView.INVALID_POSITION) {
								dialog.listView.performItemClick(
									dialog.listView.selectedView,
									pos,
									dialog.listView.getItemIdAtPosition(pos)
								)
								true
							} else false
						} else if (positiveBtn != null) {
							positiveBtn.performClick()
							true
						} else false
					}
					KeyEvent.KEYCODE_BUTTON_B -> {
						dialog.cancel()
						true
					}
					else -> false
				}
			} else false
		}

		return dialog
	}
}

/**
 * Convenience entry-point: every dialog in the app should be created via
 * `context.alertDialogBuilder()` so TV enhancements are applied automatically.
 */
fun Context.alertDialogBuilder(): AppAlertDialogBuilder = AppAlertDialogBuilder(this)
