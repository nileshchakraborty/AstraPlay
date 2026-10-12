// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.touchcontrols

import android.os.Bundle
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.LiveData
import androidx.lifecycle.Observer
import io.github.nileshchakraborty.astraplay.databinding.FragmentControlsBinding
import com.metallic.chiaki.lib.ControllerState
import io.reactivex.Observable
import io.reactivex.rxkotlin.Observables.combineLatest
import io.reactivex.subjects.BehaviorSubject
import io.reactivex.subjects.Subject

abstract class TouchControlsFragment : Fragment()
{
	protected var ownControllerState = ControllerState()
		set(value)
		{
			val diff = field != value
			field = value
			if(diff)
				ownControllerStateSubject.onNext(ownControllerState)
		}

	protected val ownControllerStateSubject: Subject<ControllerState>
			= BehaviorSubject.create<ControllerState>().also { it.onNext(ownControllerState) }

	// to delay attaching to the touchpadView until it's available
	protected val controllerStateProxy: Subject<Observable<ControllerState>>
			= BehaviorSubject.create<Observable<ControllerState>>().also { it.onNext(ownControllerStateSubject) }
	val controllerState: Observable<ControllerState> get() =
		controllerStateProxy.flatMap { it }

	var onScreenControlsEnabled: LiveData<Boolean>? = null
}

class DefaultTouchControlsFragment : TouchControlsFragment()
{
	private var _binding: FragmentControlsBinding? = null
	private val binding get() = _binding!!

	override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View =
		FragmentControlsBinding.inflate(inflater, container, false).let {
			_binding = it
			controllerStateProxy.onNext(
				combineLatest(ownControllerStateSubject, binding.touchpadView.controllerState) { a, b -> a or b }
			)
			it.root
		}

	override fun onViewCreated(view: View, savedInstanceState: Bundle?)
	{
		super.onViewCreated(view, savedInstanceState)
		binding.dpadView.stateChangeCallback = this::dpadStateChanged
		binding.crossButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_CROSS)
		binding.moonButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_MOON)
		binding.pyramidButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_PYRAMID)
		binding.boxButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_BOX)
		binding.l1ButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_L1)
		binding.r1ButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_R1)
		binding.l3ButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_L3)
		binding.r3ButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_R3)
		binding.optionsButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_OPTIONS)
		binding.shareButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_SHARE)
		binding.psButtonView.buttonPressedCallback = buttonStateChanged(ControllerState.BUTTON_PS)

		binding.l2ButtonView.buttonPressedCallback = { ownControllerState = ownControllerState.copy().apply { l2State = if(it) 255U else 0U } }
		binding.r2ButtonView.buttonPressedCallback = { ownControllerState = ownControllerState.copy().apply { r2State = if(it) 255U else 0U } }

		val quantizeStick = { f: Float ->
			(Short.MAX_VALUE * f).toInt().toShort()
		}

		binding.leftAnalogStickView.stateChangedCallback = { ownControllerState = ownControllerState.copy().apply {
			leftX = quantizeStick(it.x)
			leftY = quantizeStick(it.y)
		}}

		binding.rightAnalogStickView.stateChangedCallback = { ownControllerState = ownControllerState.copy().apply {
			rightX = quantizeStick(it.x)
			rightY = quantizeStick(it.y)
		}}

		onScreenControlsEnabled?.observe(viewLifecycleOwner, Observer {
			view.visibility = if(it) View.VISIBLE else View.GONE
		})

		val preferences = com.metallic.chiaki.common.Preferences(requireContext())
		view.alpha = preferences.touchControlsOpacity / 100.0f

		val scale = preferences.touchControlsScale / 100.0f
		if (Math.abs(scale - 1.0f) > 0.01f) {
			binding.dpadView.scaleX = scale
			binding.dpadView.scaleY = scale
			binding.faceButtonsLayout.scaleX = scale
			binding.faceButtonsLayout.scaleY = scale
			binding.leftAnalogStickView.scaleX = scale
			binding.leftAnalogStickView.scaleY = scale
			binding.rightAnalogStickView.scaleX = scale
			binding.rightAnalogStickView.scaleY = scale
		}

		applyLayoutPreset(preferences)
	}

	fun applyLayoutPreset(preferences: com.metallic.chiaki.common.Preferences) {
		when (preferences.touchControlsLayoutPreset) {
			"compact" -> {
				(binding.dpadView.layoutParams as? ViewGroup.MarginLayoutParams)?.let {
					it.leftMargin = (8 * resources.displayMetrics.density).toInt()
					it.bottomMargin = (8 * resources.displayMetrics.density).toInt()
					binding.dpadView.layoutParams = it
				}
				(binding.faceButtonsLayout.layoutParams as? ViewGroup.MarginLayoutParams)?.let {
					it.rightMargin = (16 * resources.displayMetrics.density).toInt()
					it.bottomMargin = (16 * resources.displayMetrics.density).toInt()
					binding.faceButtonsLayout.layoutParams = it
				}
				clearCustomTranslations()
			}
			"expanded" -> {
				(binding.dpadView.layoutParams as? ViewGroup.MarginLayoutParams)?.let {
					it.leftMargin = (24 * resources.displayMetrics.density).toInt()
					it.bottomMargin = (28 * resources.displayMetrics.density).toInt()
					binding.dpadView.layoutParams = it
				}
				(binding.faceButtonsLayout.layoutParams as? ViewGroup.MarginLayoutParams)?.let {
					it.rightMargin = (44 * resources.displayMetrics.density).toInt()
					it.bottomMargin = (44 * resources.displayMetrics.density).toInt()
					binding.faceButtonsLayout.layoutParams = it
				}
				clearCustomTranslations()
			}
			"custom" -> {
				applyCustomOffsets(preferences.touchControlsCustomOffsets)
			}
			else -> {
				clearCustomTranslations()
			}
		}
	}

	private fun clearCustomTranslations() {
		val views = listOf(
			binding.dpadView, binding.faceButtonsLayout,
			binding.l1ButtonView, binding.r1ButtonView,
			binding.l2ButtonView, binding.r2ButtonView,
			binding.l3ButtonView, binding.r3ButtonView,
			binding.shareButtonView, binding.optionsButtonView,
			binding.psButtonView, binding.touchpadView
		)
		views.forEach {
			it.translationX = 0f
			it.translationY = 0f
		}
	}

	fun applyCustomOffsets(offsetsStr: String) {
		if (offsetsStr.isBlank()) {
			clearCustomTranslations()
			return
		}
		val map = mutableMapOf<String, Pair<Float, Float>>()
		offsetsStr.split(";").forEach { entry ->
			val parts = entry.split(":")
			if (parts.size == 2) {
				val coords = parts[1].split(",")
				if (coords.size == 2) {
					val dx = coords[0].toFloatOrNull() ?: 0f
					val dy = coords[1].toFloatOrNull() ?: 0f
					map[parts[0]] = Pair(dx, dy)
				}
			}
		}

		val namedViews = mapOf(
			"dpad" to binding.dpadView,
			"face" to binding.faceButtonsLayout,
			"l1" to binding.l1ButtonView,
			"r1" to binding.r1ButtonView,
			"l2" to binding.l2ButtonView,
			"r2" to binding.r2ButtonView,
			"l3" to binding.l3ButtonView,
			"r3" to binding.r3ButtonView,
			"share" to binding.shareButtonView,
			"options" to binding.optionsButtonView,
			"ps" to binding.psButtonView,
			"touchpad" to binding.touchpadView
		)

		namedViews.forEach { (key, view) ->
			val offset = map[key]
			if (offset != null) {
				view.translationX = offset.first
				view.translationY = offset.second
			} else {
				view.translationX = 0f
				view.translationY = 0f
			}
		}
	}

	fun serializeCustomOffsets(): String {
		val namedViews = mapOf(
			"dpad" to binding.dpadView,
			"face" to binding.faceButtonsLayout,
			"l1" to binding.l1ButtonView,
			"r1" to binding.r1ButtonView,
			"l2" to binding.l2ButtonView,
			"r2" to binding.r2ButtonView,
			"l3" to binding.l3ButtonView,
			"r3" to binding.r3ButtonView,
			"share" to binding.shareButtonView,
			"options" to binding.optionsButtonView,
			"ps" to binding.psButtonView,
			"touchpad" to binding.touchpadView
		)
		return namedViews.map { "${it.key}:${it.value.translationX.toInt()},${it.value.translationY.toInt()}" }.joinToString(";")
	}

	private var isEditMode = false

	fun setEditMode(enabled: Boolean) {
		isEditMode = enabled
		val draggableViews = listOf(
			binding.dpadView, binding.faceButtonsLayout,
			binding.l1ButtonView, binding.r1ButtonView,
			binding.l2ButtonView, binding.r2ButtonView,
			binding.l3ButtonView, binding.r3ButtonView,
			binding.shareButtonView, binding.optionsButtonView,
			binding.psButtonView, binding.touchpadView
		)
		draggableViews.forEach { v ->
			if (enabled) {
				setupDragOnView(v)
			} else {
				v.setOnTouchListener(null)
			}
		}
	}

	private fun setupDragOnView(target: View) {
		var startX = 0f
		var startY = 0f
		var initTransX = 0f
		var initTransY = 0f

		target.setOnTouchListener { v, event ->
			when (event.actionMasked) {
				MotionEvent.ACTION_DOWN -> {
					startX = event.rawX
					startY = event.rawY
					initTransX = v.translationX
					initTransY = v.translationY
					true
				}
				MotionEvent.ACTION_MOVE -> {
					val dx = event.rawX - startX
					val dy = event.rawY - startY
					v.translationX = initTransX + dx
					v.translationY = initTransY + dy
					true
				}
				MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
					val prefs = com.metallic.chiaki.common.Preferences(requireContext())
					prefs.touchControlsLayoutPreset = "custom"
					prefs.touchControlsCustomOffsets = serializeCustomOffsets()
					true
				}
				else -> false
			}
		}
	}

	private fun dpadStateChanged(direction: DPadView.Direction?)
	{
		ownControllerState = ownControllerState.copy().apply {
			buttons = ((buttons
						and ControllerState.BUTTON_DPAD_LEFT.inv()
						and ControllerState.BUTTON_DPAD_RIGHT.inv()
						and ControllerState.BUTTON_DPAD_UP.inv()
						and ControllerState.BUTTON_DPAD_DOWN.inv())
					or when(direction)
					{
						DPadView.Direction.UP -> ControllerState.BUTTON_DPAD_UP
						DPadView.Direction.DOWN -> ControllerState.BUTTON_DPAD_DOWN
						DPadView.Direction.LEFT -> ControllerState.BUTTON_DPAD_LEFT
						DPadView.Direction.RIGHT -> ControllerState.BUTTON_DPAD_RIGHT
						DPadView.Direction.LEFT_UP -> ControllerState.BUTTON_DPAD_LEFT or ControllerState.BUTTON_DPAD_UP
						DPadView.Direction.LEFT_DOWN -> ControllerState.BUTTON_DPAD_LEFT or ControllerState.BUTTON_DPAD_DOWN
						DPadView.Direction.RIGHT_UP -> ControllerState.BUTTON_DPAD_RIGHT or ControllerState.BUTTON_DPAD_UP
						DPadView.Direction.RIGHT_DOWN -> ControllerState.BUTTON_DPAD_RIGHT or ControllerState.BUTTON_DPAD_DOWN
						null -> 0U
					})
		}
	}

	private fun buttonStateChanged(buttonMask: UInt) = { pressed: Boolean ->
		ownControllerState = ownControllerState.copy().apply {
			buttons =
				if(pressed)
					buttons or buttonMask
				else
					buttons and buttonMask.inv()

		}
	}
}