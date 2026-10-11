// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.cloudplay

import android.app.Activity
import android.app.UiModeManager
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color
import android.content.res.Configuration
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.ImageView
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import com.google.android.material.appbar.MaterialToolbar
import io.github.nileshchakraborty.astraplay.BuildConfig
import io.github.nileshchakraborty.astraplay.R
import com.metallic.chiaki.common.SecureTokenManager
import com.metallic.chiaki.common.Preferences
import com.metallic.chiaki.common.PsnTokenManager
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import kotlin.random.Random

/**
 * PSN Login Activity using astraplay relay code flow
 * Mirrors desktop app's QR login flow from gui/src/qmlbackend.cpp
 * Reference: gui/src/qmlbackend.cpp lines 3363-3525
 */
class PsnLoginActivity : AppCompatActivity()
{
	companion object
	{
		private const val TAG = "PsnLoginActivity"
		const val EXTRA_NPSSO_TOKEN = "npsso_token"
		const val RESULT_LOGIN_SUCCESS = Activity.RESULT_OK
		const val RESULT_LOGIN_CANCELLED = Activity.RESULT_CANCELED
		const val RESULT_LOGIN_FAILED = 3
	}
	
	// Phone UI
	private lateinit var codeTextView: TextView
	private lateinit var statusTextView: TextView
	private lateinit var progressBar: ProgressBar
	private lateinit var openBrowserButton: Button
	private lateinit var checkStatusButton: Button
	private lateinit var cancelButton: Button

	// TV UI
	private lateinit var tvCodeTextView: TextView
	private lateinit var tvStatusTextView: TextView
	private lateinit var tvProgressBar: ProgressBar
	private lateinit var tvCheckStatusButton: Button
	private lateinit var tvCancelButton: Button
	private lateinit var qrCodeImage: ImageView

	private lateinit var tokenManager: SecureTokenManager
	private var isOnTv: Boolean = false
	
	private var loginCode: String = ""
	private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

	// PSN login relay host baked in at build time via -PAstraPlayRelayUrl.
	// Empty when the publisher did not configure one: relay login is disabled.
	private fun relayBaseUrl(): String = BuildConfig.ASTRAPLAY_RELAY_URL.trim().trimEnd('/')
	

	override fun onCreate(savedInstanceState: Bundle?)
	{
		super.onCreate(savedInstanceState)
		setContentView(R.layout.activity_psn_login)
		
		tokenManager = SecureTokenManager(this)
		isOnTv = (getSystemService(UI_MODE_SERVICE) as UiModeManager).currentModeType == Configuration.UI_MODE_TYPE_TELEVISION

		// Setup toolbar
		val toolbar = findViewById<MaterialToolbar>(R.id.toolbar)
		setSupportActionBar(toolbar)
		supportActionBar?.apply {
			setDisplayHomeAsUpEnabled(true)
			setDisplayShowHomeEnabled(true)
			title = getString(R.string.psn_login_title)
		}
		toolbar.setNavigationOnClickListener {
			setResult(RESULT_LOGIN_CANCELLED)
			finish()
		}

		if (isOnTv) {
			// Show TV layout, hide phone layout
			findViewById<View>(R.id.phoneModeLayout).visibility = View.GONE
			findViewById<View>(R.id.tvModeLayout).visibility = View.VISIBLE

			tvCodeTextView = findViewById(R.id.tvLoginCodeText)
			tvStatusTextView = findViewById(R.id.tvStatusText)
			tvProgressBar = findViewById(R.id.tvProgressBar)
			tvCheckStatusButton = findViewById(R.id.tvCheckStatusButton)
			tvCancelButton = findViewById(R.id.tvCancelButton)
			qrCodeImage = findViewById(R.id.qrCodeImage)

			tvCheckStatusButton.setOnClickListener { checkTokenStatus() }
			tvCancelButton.setOnClickListener {
				setResult(RESULT_LOGIN_CANCELLED)
				finish()
			}

			// Yellow focus highlights for TV D-pad navigation
			val tvFocusHighlight = View.OnFocusChangeListener { v, hasFocus ->
				v.foreground = if (hasFocus)
					android.graphics.drawable.GradientDrawable().apply {
						shape = android.graphics.drawable.GradientDrawable.RECTANGLE
						cornerRadius = 24f
						setColor(0x33FFD700.toInt())
						setStroke(3, 0xCCFFD700.toInt())
					}
				else null
			}
			tvCheckStatusButton.onFocusChangeListener = tvFocusHighlight
			tvCancelButton.onFocusChangeListener = tvFocusHighlight

			tvCancelButton.requestFocusFromTouch()
		} else {
			// Phone layout
			codeTextView = findViewById(R.id.loginCodeText)
			statusTextView = findViewById(R.id.statusText)
			progressBar = findViewById(R.id.progress_bar)
			openBrowserButton = findViewById(R.id.openBrowserButton)
			checkStatusButton = findViewById(R.id.checkStatusButton)
			cancelButton = findViewById(R.id.cancelButton)

			openBrowserButton.setOnClickListener { openAstraPlayInBrowser() }
			checkStatusButton.setOnClickListener { checkTokenStatus() }
			cancelButton.setOnClickListener {
				setResult(RESULT_LOGIN_CANCELLED)
				finish()
			}
		}
		
		// Start login flow
		startLogin()
	}
	
	private fun startLogin()
	{
		if (relayBaseUrl().isEmpty()) {
			val msg = getString(R.string.psn_login_relay_not_configured)
			Log.w(TAG, msg)
			if (isOnTv) {
				tvStatusTextView.text = msg
				tvProgressBar.visibility = View.GONE
			} else {
				statusTextView.text = msg
				progressBar.visibility = View.GONE
			}
			return
		}

		val chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
		loginCode = (1..6).map { chars.random() }.joinToString("")

		if (isOnTv) {
			tvCodeTextView.text = loginCode
			loadQrCode(loginCode)
		} else {
			codeTextView.text = loginCode
		}

		Log.i(TAG, "Generated login code: $loginCode")

		scope.launch {
			val success = createAstraPlayCode(loginCode)
			if (success) {
				if (isOnTv) {
					tvStatusTextView.text = getString(R.string.psn_login_code_ready)
					tvProgressBar.visibility = View.GONE
					tvCheckStatusButton.isEnabled = true
					tvCheckStatusButton.requestFocusFromTouch()
				} else {
					statusTextView.text = getString(R.string.psn_login_code_ready)
					openBrowserButton.isEnabled = true
					checkStatusButton.isEnabled = true
				}
			} else {
				val errorMsg = getString(R.string.psn_login_server_error)
				if (isOnTv) {
					tvStatusTextView.text = errorMsg
					tvProgressBar.visibility = View.GONE
				} else {
					statusTextView.text = errorMsg
					openBrowserButton.isEnabled = false
				}
			}
		}
	}

	private fun loadQrCode(code: String)
	{
		val loginUrl = "${relayBaseUrl()}/psstream/?psstream_code=$code"
		qrCodeImage.setImageBitmap(generateQrCode(loginUrl, 440))
	}

	private fun generateQrCode(content: String, size: Int): Bitmap
	{
		val hints = mapOf(EncodeHintType.MARGIN to 1)
		val bitMatrix = QRCodeWriter().encode(content, BarcodeFormat.QR_CODE, size, size, hints)
		val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
		for (x in 0 until size) {
			for (y in 0 until size) {
				bitmap.setPixel(x, y, if (bitMatrix[x, y]) Color.BLACK else Color.WHITE)
			}
		}
		return bitmap
	}
	
	/**
	 * Create code on relay server
	 * Reference: gui/src/qmlbackend.cpp lines 3363-3440
	 */
	private suspend fun createAstraPlayCode(code: String): Boolean = withContext(Dispatchers.IO)
	{
		try
		{
			val url = URL("${relayBaseUrl()}/psstream/create-code")
			val connection = url.openConnection() as HttpURLConnection
			connection.requestMethod = "POST"
			connection.setRequestProperty("Content-Type", "application/json")
			connection.doOutput = true
			
			// Send JSON payload: {"code": "123456"}
			val jsonPayload = JSONObject().apply {
				put("code", code)
			}.toString()
			
			connection.outputStream.use { it.write(jsonPayload.toByteArray()) }
			
			val responseCode = connection.responseCode
			if (responseCode == HttpURLConnection.HTTP_OK)
			{
				val response = connection.inputStream.bufferedReader().use { it.readText() }
				val jsonResponse = JSONObject(response)
				
				if (jsonResponse.optString("result") == "success")
				{
					Log.i(TAG, "astraplay code created successfully")
					return@withContext true
				}
				else
				{
					val error = jsonResponse.optString("error", "Unknown error")
					Log.e(TAG, "astraplay server error: $error")
					return@withContext false
				}
			}
			else
			{
				Log.e(TAG, "HTTP error creating code: $responseCode")
				return@withContext false
			}
		}
		catch (e: Exception)
		{
			Log.e(TAG, "Exception creating astraplay code", e)
			return@withContext false
		}
	}
	
	/**
	 * Check token status when user clicks the button
	 */
	private fun checkTokenStatus()
	{
		if (isOnTv) {
			tvCheckStatusButton.isEnabled = false
			tvProgressBar.visibility = View.VISIBLE
			tvStatusTextView.text = getString(R.string.psn_login_checking_status)
		} else {
			checkStatusButton.isEnabled = false
			progressBar.visibility = View.VISIBLE
			statusTextView.text = getString(R.string.psn_login_checking_status)
		}

		scope.launch {
			val token = checkAstraPlayStatus(loginCode)

			if (isOnTv) {
				tvProgressBar.visibility = View.GONE
				tvCheckStatusButton.isEnabled = true
				tvCheckStatusButton.requestFocusFromTouch()
			} else {
				progressBar.visibility = View.GONE
				checkStatusButton.isEnabled = true
			}

			if (token != null) {
				Log.i(TAG, "NPSSO token received from relay server")
				onLoginSuccess(token)
			} else {
				val msg = getString(R.string.psn_login_not_complete)
				if (isOnTv) {
					tvStatusTextView.text = msg
				} else {
					statusTextView.text = msg
				}
				Toast.makeText(this@PsnLoginActivity, R.string.psn_login_not_complete_toast, Toast.LENGTH_SHORT).show()
			}
		}
	}
	
	/**
	 * Check relay server for token status
	 * Reference: gui/src/qmlbackend.cpp lines 3442-3525
	 */
	private suspend fun checkAstraPlayStatus(code: String): String? = withContext(Dispatchers.IO)
	{
		try
		{
			val url = URL("${relayBaseUrl()}/psstream/get-tokens")
			val connection = url.openConnection() as HttpURLConnection
			connection.requestMethod = "POST"
			connection.setRequestProperty("Content-Type", "application/json")
			connection.doOutput = true
			
			// Send JSON payload: {"code": "ABC123"}
			val jsonPayload = JSONObject().apply {
				put("code", code)
			}.toString()
			
			Log.d(TAG, "Checking token status for code: $code")
			connection.outputStream.use { it.write(jsonPayload.toByteArray()) }
			
			val responseCode = connection.responseCode
			Log.d(TAG, "Response code: $responseCode")
			
			if (responseCode == HttpURLConnection.HTTP_OK)
			{
				val response = connection.inputStream.bufferedReader().use { it.readText() }
				Log.d(TAG, "Response body: $response")
				val jsonResponse = JSONObject(response)
				
				val result = jsonResponse.optString("result")
				Log.d(TAG, "Result field: $result")
				
				when (result)
				{
					"success" -> {
						// Token is ready
						val npsso = jsonResponse.optString("npsso")
						if (npsso.isNotEmpty())
						{
							Log.i(TAG, "Received NPSSO token (length: ${npsso.length})")
							return@withContext npsso
						}
						else
						{
							Log.w(TAG, "Success result but empty npsso field")
						}
					}
					"pending", "" -> {
						// Still waiting for user to complete login or code not found yet
						Log.d(TAG, "Token status: pending or not found")
						return@withContext null
					}
					"error" -> {
						val error = jsonResponse.optString("error", "Unknown error")
						Log.e(TAG, "astraplay error: $error")
						return@withContext null
					}
					else -> {
						Log.w(TAG, "Unknown result value: $result")
					}
				}
			}
			else
			{
				Log.w(TAG, "HTTP response code: $responseCode")
				// Try to read error response
				try {
					val errorResponse = connection.errorStream?.bufferedReader()?.use { it.readText() }
					if (errorResponse != null) {
						Log.w(TAG, "Error response: $errorResponse")
					}
				} catch (e: Exception) {
					Log.w(TAG, "Could not read error stream", e)
				}
			}
			
			return@withContext null
		}
		catch (e: Exception)
		{
			Log.e(TAG, "Exception checking astraplay status", e)
			return@withContext null
		}
	}
	
	/**
	 * Open relay URL with the login code in browser
	 * Reference: gui/src/qml/QRLoginDialog.qml line 225
	 */
	private fun openAstraPlayInBrowser()
	{
		try
		{
			val relayUrl = "${relayBaseUrl()}/psstream/?psstream_code=$loginCode"
			val intent = Intent(Intent.ACTION_VIEW, Uri.parse(relayUrl))
			startActivity(intent)
			Log.i(TAG, "Opened relay URL in browser: $relayUrl")
			statusTextView.text = getString(R.string.psn_login_browser_opened)
			
			// Highlight the Check Status button by changing it to filled style
			checkStatusButton.apply {
				setBackgroundColor(getColor(com.google.android.material.R.color.design_default_color_primary))
				setTextColor(getColor(android.R.color.white))
				elevation = 8f
			}
			
			// Dim the open browser button since it's already been used
			openBrowserButton.alpha = 0.6f
		}
		catch (e: Exception)
		{
			Log.e(TAG, "Failed to open browser", e)
			Toast.makeText(this, getString(R.string.psn_login_external_browser_error), Toast.LENGTH_SHORT).show()
		}
	}
	
	private fun onLoginSuccess(token: String)
	{
		// Save NPSSO token securely. If the secure store is unavailable the token would be
		// forgotten by the next screen — reporting success anyway put the user in a silent
		// login loop (log in, see "success", land back on the login screen). Surface it.
		if(!tokenManager.saveNpssoToken(token))
		{
			Log.e(TAG, "PSN login: token could not be stored; not reporting success")
			Toast.makeText(this, getString(R.string.psn_login_token_store_error), Toast.LENGTH_LONG).show()
			return
		}

		// Show progress and exchange for Remote Play tokens on background thread (matches Qt app flow)
		Toast.makeText(this, "Setting up PSN (Cloud + Remote Play)...", Toast.LENGTH_SHORT).show()
		
		Thread {
			val preferences = Preferences(this)
			val psnTokenManager = PsnTokenManager(preferences)
			val exchangeSuccess = psnTokenManager.exchangeNpssoForTokens(token)
			
			runOnUiThread {
				if(exchangeSuccess)
				{
					Log.i(TAG, "PSN login complete: NPSSO + Remote Play tokens saved")
				}
				else
				{
					Log.w(TAG, "PSN login: NPSSO saved, but Remote Play token exchange failed")
				}
				
				// Return success (NPSSO is saved; RP tokens saved if exchange succeeded)
				val resultIntent = Intent().apply {
					putExtra(EXTRA_NPSSO_TOKEN, token)
				}
				setResult(RESULT_LOGIN_SUCCESS, resultIntent)
				Toast.makeText(this, getString(R.string.psn_login_success), Toast.LENGTH_SHORT).show()
				finish()
			}
		}.start()
	}
	
	override fun onDestroy()
	{
		super.onDestroy()
		scope.cancel()
	}
	
	override fun onBackPressed()
	{
		setResult(RESULT_LOGIN_CANCELLED)
		super.onBackPressed()
	}
}
