package com.metallic.chiaki.stream

import android.content.Context
import android.util.AttributeSet
import android.widget.FrameLayout

// see ExoPlayer's AspectRatioFrameLayout
class AspectRatioFrameLayout @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null): FrameLayout(context, attrs)
{
	companion object
	{
		private const val MAX_ASPECT_RATIO_DEFORMATION_FRACTION = 0.01f
	}

	var aspectRatio = 0f
		set(value)
		{
			if(field != value)
			{
				field = value
				requestLayout()
			}
		}

	var mode: TransformMode = TransformMode.FIT
		set(value)
		{
			if(field != value)
			{
				field = value
				requestLayout()
			}
		}

	override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int)
	{
		super.onMeasure(widthMeasureSpec, heightMeasureSpec)
		if(aspectRatio <= 0)
		{
			// Aspect ratio not set.
			return
		}
		var width = measuredWidth
		var height = measuredHeight
		val viewAspectRatio = width.toFloat() / height
		val targetAspect = when(mode)
		{
			TransformMode.PRESET_16_10 -> 16f / 10f
			TransformMode.PRESET_20_9 -> 20f / 9f
			else -> aspectRatio
		}
		val aspectDeformation = targetAspect / viewAspectRatio - 1
		if(kotlin.math.abs(aspectDeformation) <= MAX_ASPECT_RATIO_DEFORMATION_FRACTION)
			return
		when(mode)
		{
			TransformMode.ZOOM, TransformMode.PRESET_16_10, TransformMode.PRESET_20_9 -> 
				if(aspectDeformation > 0)
					width = (height * targetAspect).toInt()
				else
					height = (width / targetAspect).toInt()
			TransformMode.FIT ->
				if(aspectDeformation > 0)
					height = (width / targetAspect).toInt()
				else
					width = (height * targetAspect).toInt()
			TransformMode.STRETCH -> {}
		}
		super.onMeasure(
			MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
			MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
		)
	}
}
