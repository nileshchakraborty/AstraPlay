// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.regist

import android.content.Intent
import android.os.Bundle
import android.util.Base64
import android.view.View
import android.view.Window
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import io.github.nileshchakraborty.astraplay.R
import com.metallic.chiaki.common.ext.RevealActivity
import io.github.nileshchakraborty.astraplay.databinding.ActivityRegistBinding
import com.metallic.chiaki.lib.RegistInfo
import com.metallic.chiaki.lib.Target
import java.lang.IllegalArgumentException

class RegistActivity: AppCompatActivity(), RevealActivity
{
	companion object
	{
		const val EXTRA_HOST = "regist_host"
		const val EXTRA_BROADCAST = "regist_broadcast"
		const val EXTRA_ASSIGN_MANUAL_HOST_ID = "assign_manual_host_id"

		private const val PIN_LENGTH = 8

		private const val REQUEST_REGIST = 1
		private const val REQUEST_PSN_LOGIN_AUTOFILL = 2
	}

	private lateinit var viewModel: RegistViewModel
	private lateinit var binding: ActivityRegistBinding
	private lateinit var preferences: com.metallic.chiaki.common.Preferences

	override val revealWindow: Window get() = window
	override val revealIntent: Intent get() = intent
	override val revealRootLayout: View get() = binding.rootLayout

	override fun onCreate(savedInstanceState: Bundle?)
	{
		super.onCreate(savedInstanceState)
		binding = ActivityRegistBinding.inflate(layoutInflater)
		setContentView(binding.root)
		handleReveal()

		preferences = com.metallic.chiaki.common.Preferences(this)
		viewModel = ViewModelProvider(this).get(RegistViewModel::class.java)

		binding.hostEditText.setText(intent.getStringExtra(EXTRA_HOST) ?: "255.255.255.255")
		binding.broadcastCheckBox.isChecked = intent.getBooleanExtra(EXTRA_BROADCAST, true)

		// Auto-fill Account ID from existing PSN login if available
		applySavedAccountId()

		binding.psnLoginAutoFillButton.setOnClickListener {
			val loginIntent = Intent(this, com.metallic.chiaki.cloudplay.PsnLoginActivity::class.java)
			startActivityForResult(loginIntent, REQUEST_PSN_LOGIN_AUTOFILL)
		}

		binding.registButton.setOnClickListener { doRegist() }

		binding.ps4VersionRadioGroup.check(when(viewModel.ps4Version.value ?: RegistViewModel.ConsoleVersion.PS5) {
			RegistViewModel.ConsoleVersion.PS5 -> R.id.ps5RadioButton
			RegistViewModel.ConsoleVersion.PS4_GE_8 -> R.id.ps4VersionGE8RadioButton
			RegistViewModel.ConsoleVersion.PS4_GE_7 -> R.id.ps4VersionGE7RadioButton
			RegistViewModel.ConsoleVersion.PS4_LT_7 -> R.id.ps4VersionLT7RadioButton
		})

		binding.ps4VersionRadioGroup.setOnCheckedChangeListener { _, checkedId ->
			viewModel.ps4Version.value = when(checkedId)
			{
				R.id.ps5RadioButton -> RegistViewModel.ConsoleVersion.PS5
				R.id.ps4VersionGE8RadioButton -> RegistViewModel.ConsoleVersion.PS4_GE_8
				R.id.ps4VersionGE7RadioButton -> RegistViewModel.ConsoleVersion.PS4_GE_7
				R.id.ps4VersionLT7RadioButton -> RegistViewModel.ConsoleVersion.PS4_LT_7
				else -> RegistViewModel.ConsoleVersion.PS5
			}
		}

		viewModel.ps4Version.observe(this, Observer {
			binding.psnAccountIdHelpGroup.visibility = if(it == RegistViewModel.ConsoleVersion.PS4_LT_7) View.GONE else View.VISIBLE
			binding.psnIdTextInputLayout.hint = getString(when(it!!)
			{
				RegistViewModel.ConsoleVersion.PS4_LT_7 -> R.string.hint_regist_psn_online_id
				else -> R.string.hint_regist_psn_account_id
			})
			binding.pinHelpBeforeTextView.setText(if(it.isPS5) R.string.regist_pin_instructions_ps5_before else R.string.regist_pin_instructions_ps4_before)
			binding.pinHelpNavigationTextView.setText(if(it.isPS5) R.string.regist_pin_instructions_ps5_navigation else R.string.regist_pin_instructions_ps4_navigation)
		})
	}

	private fun applySavedAccountId()
	{
		val savedId = preferences.psnAccountId
		if (savedId.isNotEmpty()) {
			binding.psnIdEditText.setText(savedId)
			binding.psnLoginAutoFillButton.text = getString(R.string.psn_account_id_autofilled)
			binding.psnLoginAutoFillButton.alpha = 0.85f
		}
	}

	private fun doRegist()
	{
		val ps4Version = viewModel.ps4Version.value ?: RegistViewModel.ConsoleVersion.PS5

		val host = binding.hostEditText.text.toString().trim()
		val hostValid = host.isNotEmpty()
		val broadcast = binding.broadcastCheckBox.isChecked

		val psnId = binding.psnIdEditText.text.toString().trim()
		val psnOnlineId: String? = if(ps4Version == RegistViewModel.ConsoleVersion.PS4_LT_7) psnId else null
		val psnAccountId: ByteArray? =
			if(ps4Version != RegistViewModel.ConsoleVersion.PS4_LT_7)
				try { Base64.decode(psnId, Base64.DEFAULT) } catch(e: IllegalArgumentException) { null }
			else
				null
		val psnIdValid = when(ps4Version)
		{
			RegistViewModel.ConsoleVersion.PS4_LT_7 -> psnOnlineId?.isNotEmpty() ?: false
			else -> psnAccountId != null && psnAccountId.size == RegistInfo.ACCOUNT_ID_SIZE
		}


		val pin = binding.pinEditText.text.toString()
		val pinValid = pin.length == PIN_LENGTH

		binding.hostEditText.error = if(!hostValid) getString(R.string.entered_host_invalid) else null
		binding.psnIdEditText.error =
			if(!psnIdValid)
				getString(when(ps4Version)
				{
					RegistViewModel.ConsoleVersion.PS4_LT_7 -> R.string.regist_psn_online_id_invalid
					else -> R.string.regist_psn_account_id_invalid
				})
			else
				null
		binding.pinEditText.error = if(!pinValid) getString(R.string.regist_pin_invalid, PIN_LENGTH) else null

		if(!hostValid || !psnIdValid || !pinValid)
			return

		val target = when(ps4Version)
		{
			RegistViewModel.ConsoleVersion.PS5 -> Target.PS5_1
			RegistViewModel.ConsoleVersion.PS4_GE_8 -> Target.PS4_10
			RegistViewModel.ConsoleVersion.PS4_GE_7 -> Target.PS4_9
			RegistViewModel.ConsoleVersion.PS4_LT_7 -> Target.PS4_8
		}

		val registInfo = RegistInfo(target, host, broadcast, psnOnlineId, psnAccountId, pin.toInt())

		Intent(this, RegistExecuteActivity::class.java).also {
			it.putExtra(RegistExecuteActivity.EXTRA_REGIST_INFO, registInfo)
			if(intent.hasExtra(EXTRA_ASSIGN_MANUAL_HOST_ID))
				it.putExtra(RegistExecuteActivity.EXTRA_ASSIGN_MANUAL_HOST_ID, intent.getLongExtra(EXTRA_ASSIGN_MANUAL_HOST_ID, 0L))
			startActivityForResult(it, REQUEST_REGIST)
		}
	}

	override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?)
	{
		super.onActivityResult(requestCode, resultCode, data)
		if(requestCode == REQUEST_REGIST && resultCode == RESULT_OK)
			finish()
		else if(requestCode == REQUEST_PSN_LOGIN_AUTOFILL && resultCode == RESULT_OK)
		{
			applySavedAccountId()
			android.widget.Toast.makeText(this, R.string.psn_account_id_autofilled, android.widget.Toast.LENGTH_SHORT).show()
		}
	}
}