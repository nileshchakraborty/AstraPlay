// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.cloudplay

import android.annotation.SuppressLint
import android.app.Activity
import android.app.UiModeManager
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.text.InputType
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.View
import android.webkit.CookieManager
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputEditText
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import io.github.nileshchakraborty.astraplay.BuildConfig
import io.github.nileshchakraborty.astraplay.R
import com.metallic.chiaki.common.SecureTokenManager
import com.metallic.chiaki.common.Preferences
import com.metallic.chiaki.common.PsnTokenManager
import kotlinx.coroutines.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean

/**
 * PSN Login Activity supporting:
 * 1. Direct In-App WebView login (default for Phones and Tablets) with automatic redirect & NPSSO capture.
 * 2. Cross-device Phone Relay via QR code (default for Android TV 10-foot UI, and accessible as alternate view).
 * Mirrors desktop Qt and iOS hybrid login flows.
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

		private const val SONY_USER_AGENT =
			"Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36"
	}

	// Mode Containers
	private lateinit var layoutWebView: View
	private lateinit var layoutPhoneRelay: View

	// WebView UI components
	private lateinit var loginWebView: WebView
	private lateinit var webViewProgressBar: ProgressBar
	private lateinit var webViewLoadingOverlay: LinearLayout
	private lateinit var webViewStatusText: TextView
	private lateinit var btnSwitchToPhoneRelay: MaterialButton
	private lateinit var btnOpenBrowserFromWebView: MaterialButton
	private lateinit var btnManualTokenFromWebView: MaterialButton
	private lateinit var btnCancelWebView: MaterialButton

	// Phone Relay UI components
	private lateinit var codeTextView: TextView
	private lateinit var statusTextView: TextView
	private lateinit var progressBar: ProgressBar
	private lateinit var openBrowserButton: MaterialButton
	private lateinit var checkStatusButton: MaterialButton
	private lateinit var configureRelayButton: MaterialButton
	private lateinit var manualTokenButton: MaterialButton
	private lateinit var cancelButton: MaterialButton
	private lateinit var qrCodeImage: ImageView

	private lateinit var toolbar: MaterialToolbar

	private lateinit var tokenManager: SecureTokenManager
	private lateinit var preferences: Preferences
	private var embeddedRelayServer: EmbeddedRelayServer? = null
	private var isOnTv: Boolean = false

	private var loginCode: String = ""
	private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
	private var autoPollJob: Job? = null
	private val isLoginHandled = AtomicBoolean(false)
	private val isPasskeyDialogShown = AtomicBoolean(false)

	private fun relayBaseUrl(): String = preferences.getEffectiveRelayUrl()

	private fun isHandheldOrTv(): Boolean {
		if (isOnTv) return true
		try {
			for (id in InputDevice.getDeviceIds()) {
				val dev = InputDevice.getDevice(id) ?: continue
				val sources = dev.sources
				if ((sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
					(sources and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
					if (!dev.isVirtual) return true
				}
			}
		} catch (_: Exception) {}

		val model = android.os.Build.MODEL.lowercase()
		return model.contains("odin") || model.contains("retroid") || model.contains("gcloud") || model.contains("abxylute")
	}

	@SuppressLint("SetJavaScriptEnabled")
	override fun onCreate(savedInstanceState: Bundle?)
	{
		super.onCreate(savedInstanceState)
		setContentView(R.layout.activity_psn_login)

		tokenManager = SecureTokenManager(this)
		preferences = Preferences(this)
		isOnTv = (getSystemService(UI_MODE_SERVICE) as UiModeManager).currentModeType == Configuration.UI_MODE_TYPE_TELEVISION

		// Start local embedded relay server if no remote relay is explicitly configured
		if (preferences.psnRelayUrl.isEmpty()) {
			try {
				embeddedRelayServer = EmbeddedRelayServer.start(this)
				Log.i(TAG, "Embedded relay server started at ${embeddedRelayServer?.serverUrl()}")
			} catch (e: Exception) {
				Log.e(TAG, "Failed to start EmbeddedRelayServer", e)
			}
		}

		bindViews()
		setupToolbar()
		setupWebView()
		setupRelayActions()

		// Prepare the relay code so both direct and QR flows are ready immediately
		prepareRelayCode()

		if (isHandheldOrTv()) {
			showPhoneRelayView()
		} else {
			showWebView()
		}
	}

	private fun bindViews()
	{
		toolbar = findViewById(R.id.toolbar)

		layoutWebView = findViewById(R.id.layoutWebView)
		layoutPhoneRelay = findViewById(R.id.layoutPhoneRelay)

		// WebView components
		loginWebView = findViewById(R.id.loginWebView)
		webViewProgressBar = findViewById(R.id.webViewProgressBar)
		webViewLoadingOverlay = findViewById(R.id.webViewLoadingOverlay)
		webViewStatusText = findViewById(R.id.webViewStatusText)
		btnSwitchToPhoneRelay = findViewById(R.id.btnSwitchToPhoneRelay)
		btnOpenBrowserFromWebView = findViewById(R.id.btnOpenBrowserFromWebView)
		btnManualTokenFromWebView = findViewById(R.id.btnManualTokenFromWebView)
		btnCancelWebView = findViewById(R.id.btnCancelWebView)

		// Relay components
		codeTextView = findViewById(R.id.loginCodeText)
		statusTextView = findViewById(R.id.statusText)
		progressBar = findViewById(R.id.progressBar)
		openBrowserButton = findViewById(R.id.openBrowserButton)
		checkStatusButton = findViewById(R.id.checkStatusButton)
		configureRelayButton = findViewById(R.id.configureRelayButton)
		manualTokenButton = findViewById(R.id.manualTokenButton)
		cancelButton = findViewById(R.id.cancelButton)
		qrCodeImage = findViewById(R.id.qrCodeImage)
	}

	private fun setupToolbar()
	{
		toolbar.setNavigationOnClickListener {
			setResult(RESULT_LOGIN_CANCELLED)
			finish()
		}
	}

	@SuppressLint("SetJavaScriptEnabled")
	private fun setupWebView()
	{
		loginWebView.settings.apply {
			javaScriptEnabled = true
			domStorageEnabled = true
			databaseEnabled = true
			useWideViewPort = true
			loadWithOverviewMode = true
			setSupportZoom(false)
			builtInZoomControls = false
			userAgentString = SONY_USER_AGENT
		}

		val cookieManager = CookieManager.getInstance()
		cookieManager.setAcceptCookie(true)
		cookieManager.setAcceptThirdPartyCookies(loginWebView, true)

		loginWebView.webChromeClient = object : WebChromeClient() {
			override fun onProgressChanged(view: WebView?, newProgress: Int) {
				if (newProgress in 1..99) {
					webViewProgressBar.visibility = View.VISIBLE
					webViewProgressBar.progress = newProgress
				} else {
					webViewProgressBar.visibility = View.GONE
				}
			}

			override fun onConsoleMessage(consoleMessage: android.webkit.ConsoleMessage?): Boolean {
				val msg = consoleMessage?.message() ?: return false
				if (msg.contains("passkey_unsupported") || (msg.contains("CORS") && msg.contains("authz/v3/oauth/token"))) {
					Log.w(TAG, "WebView console detected passkey/CORS issue: $msg")
					showPasskeyFallbackDialog()
				}
				return super.onConsoleMessage(consoleMessage)
			}
		}

		loginWebView.webViewClient = object : WebViewClient() {
			override fun shouldOverrideUrlLoading(view: WebView?, request: WebResourceRequest?): Boolean {
				val url = request?.url?.toString() ?: return false
				return handlePotentialLoginRedirect(url, view)
			}

			@Deprecated("Deprecated in Java")
			override fun shouldOverrideUrlLoading(view: WebView?, url: String?): Boolean {
				if (url == null) return false
				return handlePotentialLoginRedirect(url, view)
			}

			override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
				super.onPageStarted(view, url, favicon)
				if (url != null) {
					handlePotentialLoginRedirect(url, view)
				}
			}

			override fun onPageFinished(view: WebView?, url: String?) {
				super.onPageFinished(view, url)
				if (url == null || isLoginHandled.get()) return

				// 1. Check cookies for NPSSO
				checkCookiesForNpsso()

				// 2. If at ssocookie endpoint, extract text from JSON body
				if (url.contains("ca.account.sony.com/api/v1/ssocookie")) {
					view?.evaluateJavascript("(function() { return document.body.innerText; })();") { text ->
						if (text != null && !isLoginHandled.get()) {
							val npsso = sanitizeNpsso(text)
							if (npsso.length == 64) {
								Log.i(TAG, "Extracted NPSSO from ssocookie page content")
								onLoginSuccess(npsso)
							}
						}
					}
				}
			}
		}

		val focusHighlight = createFocusHighlight()
		btnSwitchToPhoneRelay.onFocusChangeListener = focusHighlight
		btnOpenBrowserFromWebView.onFocusChangeListener = focusHighlight
		btnManualTokenFromWebView.onFocusChangeListener = focusHighlight
		btnCancelWebView.onFocusChangeListener = focusHighlight

		btnSwitchToPhoneRelay.setOnClickListener {
			showPhoneRelayView()
		}

		btnOpenBrowserFromWebView.setOnClickListener {
			openAstraPlayInBrowser()
		}

		btnManualTokenFromWebView.setOnClickListener {
			showManualTokenDialog()
		}

		btnCancelWebView.setOnClickListener {
			setResult(RESULT_LOGIN_CANCELLED)
			finish()
		}
	}

	private fun handlePotentialLoginRedirect(url: String, view: WebView?): Boolean
	{
		if (isLoginHandled.get()) return true

		if (url.contains("passkey_unsupported")) {
			Log.w(TAG, "Passkey unsupported page detected in WebView: $url")
			showPasskeyFallbackDialog()
			return false
		}

		if (url.startsWith(PsnAuthConstants.REDIRECT_URI)) {
			Log.i(TAG, "OAuth redirect captured: $url")
			showWebViewOverlay(getString(R.string.psn_login_authenticating))

			// Check if NPSSO is already in CookieManager
			val cookieNpsso = getNpssoFromCookieManager()
			if (cookieNpsso != null) {
				Log.i(TAG, "NPSSO found directly in cookies on redirect")
				onLoginSuccess(cookieNpsso)
				return true
			}

			// Navigate webView to ssocookie endpoint to populate cookie and body
			Log.i(TAG, "Navigating to ssocookie endpoint to obtain token...")
			view?.loadUrl(PsnAuthConstants.SSOCOOKIE_ENDPOINT)

			// Fast parallel background HTTP fetch using existing session cookies
			scope.launch(Dispatchers.IO) {
				val token = fetchNpssoViaHttp()
				if (token != null && !isLoginHandled.get()) {
					withContext(Dispatchers.Main) {
						onLoginSuccess(token)
					}
				}
			}
			return true
		}
		return false
	}

	private fun checkCookiesForNpsso()
	{
		if (isLoginHandled.get()) return
		val npsso = getNpssoFromCookieManager()
		if (npsso != null) {
			Log.i(TAG, "NPSSO detected in CookieManager during page load")
			onLoginSuccess(npsso)
		}
	}

	private fun getNpssoFromCookieManager(): String?
	{
		try {
			val cookies = CookieManager.getInstance().getCookie(PsnAuthConstants.ACCOUNT_BASE) ?: return null
			val match = Regex("npsso=([a-zA-Z0-9_-]{64})").find(cookies)
			return match?.groupValues?.get(1)
		} catch (e: Exception) {
			Log.d(TAG, "Failed reading CookieManager cookies: ${e.message}")
			return null
		}
	}

	private fun fetchNpssoViaHttp(): String?
	{
		try {
			val cookies = CookieManager.getInstance().getCookie(PsnAuthConstants.ACCOUNT_BASE) ?: ""
			val url = URL(PsnAuthConstants.SSOCOOKIE_ENDPOINT)
			val conn = url.openConnection() as HttpURLConnection
			conn.connectTimeout = 6000
			conn.readTimeout = 6000
			conn.requestMethod = "GET"
			conn.setRequestProperty("User-Agent", SONY_USER_AGENT)
			if (cookies.isNotEmpty()) {
				conn.setRequestProperty("Cookie", cookies)
			}

			val responseCode = conn.responseCode
			if (responseCode == HttpURLConnection.HTTP_OK) {
				val body = conn.inputStream.bufferedReader().use { it.readText() }
				val npsso = sanitizeNpsso(body)
				if (npsso.length == 64) {
					Log.i(TAG, "Fetched NPSSO successfully via HTTP background request")
					return npsso
				}
			}
		} catch (e: Exception) {
			Log.d(TAG, "HTTP fetch for ssocookie failed: ${e.message}")
		}
		return null
	}

	private fun buildSonyOAuthUrl(): String
	{
		var duid = preferences.psnDuid
		if (duid.isEmpty()) {
			duid = DuidUtil.generateDuid()
			preferences.psnDuid = duid
		}
		val cid = UUID.randomUUID().toString()

		return buildString {
			append(PsnAuthConstants.AUTHORIZE_ENDPOINT_V3)
			append("?client_id=").append(PsnAuthConstants.CLIENT_ID)
			append("&redirect_uri=").append(Uri.encode(PsnAuthConstants.REDIRECT_URI))
			append("&scope=").append(Uri.encode(PsnAuthConstants.SCOPES))
			append("&response_type=code")
			append("&service_entity=urn:service-entity:psn")
			append("&access_type=offline")
			append("&smcid=remoteplay")
			append("&layout_type=popup")
			append("&PlatformPrivacyWs1=minimal")
			append("&no_captcha=true")
			append("&duid=").append(Uri.encode(duid))
			append("&cid=").append(Uri.encode(cid))
		}
	}

	private fun showWebView()
	{
		isPasskeyDialogShown.set(false)
		layoutWebView.visibility = View.VISIBLE
		layoutPhoneRelay.visibility = View.GONE
		toolbar.title = getString(R.string.psn_login_direct_title)

		if (loginWebView.url.isNullOrEmpty()) {
			val url = buildSonyOAuthUrl()
			Log.i(TAG, "Loading Sony OAuth URL in WebView: $url")
			loginWebView.loadUrl(url)
		}
	}

	private fun showPhoneRelayView()
	{
		layoutWebView.visibility = View.GONE
		layoutPhoneRelay.visibility = View.VISIBLE
		toolbar.title = getString(R.string.psn_login_title)

		if (isOnTv) {
			checkStatusButton.requestFocus()
		}
	}

	private fun openAstraPlayInBrowser()
	{
		try
		{
			val baseUrl = embeddedRelayServer?.localUrl() ?: relayBaseUrl()
			val relayUrl = "$baseUrl/psstream/?psstream_code=$loginCode"
			val intent = Intent(Intent.ACTION_VIEW, Uri.parse(relayUrl))
			startActivity(intent)
			Log.i(TAG, "Opened relay URL in browser: $relayUrl")
			Toast.makeText(this, "Complete sign-in in browser, then return here", Toast.LENGTH_LONG).show()
		}
		catch (e: Exception)
		{
			Log.e(TAG, "Failed to open browser", e)
			Toast.makeText(this, getString(R.string.psn_login_external_browser_error), Toast.LENGTH_SHORT).show()
		}
	}

	private fun showPasskeyFallbackDialog()
	{
		if (isPasskeyDialogShown.compareAndSet(false, true) && !isLoginHandled.get()) {
			runOnUiThread {
				MaterialAlertDialogBuilder(this)
					.setTitle("Passkey Sign-in Detected")
					.setMessage("Your PSN account uses a Passkey (Face ID / Fingerprint). Embedded web views on this device cannot authenticate hardware passkeys.\n\nPlease switch to 'Login via Phone' to scan the QR code with your phone where your passkey is stored.")
					.setPositiveButton("Switch to Phone (QR Code)") { _, _ ->
						showPhoneRelayView()
					}
					.setNegativeButton(R.string.action_cancel, null)
					.show()
			}
		}
	}

	private fun showWebViewOverlay(message: String)
	{
		webViewStatusText.text = message
		webViewLoadingOverlay.visibility = View.VISIBLE
	}

	private fun setupRelayActions()
	{
		// openBrowserButton now flips to WebView mode
		openBrowserButton.setOnClickListener { showWebView() }
		checkStatusButton.setOnClickListener { checkTokenStatus(manual = true) }
		configureRelayButton.setOnClickListener { showConfigureRelayDialog() }
		manualTokenButton.setOnClickListener { showManualTokenDialog() }
		cancelButton.setOnClickListener {
			setResult(RESULT_LOGIN_CANCELLED)
			finish()
		}

		val focusHighlight = createFocusHighlight()
		checkStatusButton.onFocusChangeListener = focusHighlight
		openBrowserButton.onFocusChangeListener = focusHighlight
		manualTokenButton.onFocusChangeListener = focusHighlight
		configureRelayButton.onFocusChangeListener = focusHighlight
		cancelButton.onFocusChangeListener = focusHighlight
	}

	private fun createFocusHighlight(): View.OnFocusChangeListener {
		return View.OnFocusChangeListener { v, hasFocus ->
			v.foreground = if (hasFocus)
				android.graphics.drawable.GradientDrawable().apply {
					shape = android.graphics.drawable.GradientDrawable.RECTANGLE
					cornerRadius = 24f
					setColor(0x33FFD700.toInt())
					setStroke(3, 0xCCFFD700.toInt())
				}
			else null
		}
	}

	private fun prepareRelayCode()
	{
		autoPollJob?.cancel()

		val effectiveRelay = relayBaseUrl()
		if (effectiveRelay.isEmpty()) {
			statusTextView.text = getString(R.string.psn_login_relay_unconfigured_prompt)
			progressBar.visibility = View.GONE
			qrCodeImage.visibility = View.GONE
			codeTextView.text = "------"
			return
		}

		val chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
		loginCode = (1..6).map { chars.random() }.joinToString("")
		codeTextView.text = loginCode

		loadQrCode(loginCode)
		embeddedRelayServer?.createCode(loginCode)

		scope.launch {
			val success = createAstraPlayCode(loginCode)
			if (success || embeddedRelayServer != null) {
				statusTextView.text = getString(R.string.psn_login_waiting_phone)
				progressBar.visibility = View.VISIBLE
				checkStatusButton.isEnabled = true
				openBrowserButton.isEnabled = true
				startAutoPolling()
			} else {
				val errorMsg = getString(R.string.psn_login_server_error)
				statusTextView.text = "$errorMsg ($effectiveRelay)"
				progressBar.visibility = View.GONE
				openBrowserButton.isEnabled = false
			}
		}
	}

	private fun startAutoPolling()
	{
		autoPollJob?.cancel()
		autoPollJob = scope.launch {
			while (isActive && !isLoginHandled.get()) {
				delay(2500)
				val token = checkAstraPlayStatus(loginCode)
				if (token != null) {
					Log.i(TAG, "Auto-poller detected NPSSO token")
					onLoginSuccess(token)
					break
				}
			}
		}
	}

	private fun loadQrCode(code: String)
	{
		val loginUrl = "${relayBaseUrl()}/psstream/?psstream_code=$code"
		try {
			val bitmap = generateQrCode(loginUrl, 440)
			qrCodeImage.setImageBitmap(bitmap)
			qrCodeImage.visibility = View.VISIBLE
		} catch (e: Exception) {
			Log.e(TAG, "Failed to render QR code bitmap", e)
		}
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

	private fun showConfigureRelayDialog()
	{
		val input = EditText(this).apply {
			hint = getString(R.string.psn_login_relay_dialog_hint)
			inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI
			setText(preferences.psnRelayUrl.ifEmpty { BuildConfig.ASTRAPLAY_RELAY_URL })
			setSelectAllOnFocus(true)
		}
		val container = FrameLayout(this).apply {
			val padding = (20 * resources.displayMetrics.density).toInt()
			setPadding(padding, padding / 2, padding, 0)
			addView(input)
		}

		MaterialAlertDialogBuilder(this)
			.setTitle(R.string.psn_login_relay_dialog_title)
			.setMessage(R.string.preferences_psn_relay_url_dialog_message)
			.setView(container)
			.setPositiveButton(R.string.action_ok) { _, _ ->
				val url = input.text.toString().trim()
				if (url.isNotEmpty() && !url.startsWith("http://") && !url.startsWith("https://")) {
					Toast.makeText(this, R.string.psn_login_invalid_url, Toast.LENGTH_SHORT).show()
					return@setPositiveButton
				}
				preferences.psnRelayUrl = url
				Toast.makeText(this, "Relay URL saved", Toast.LENGTH_SHORT).show()
				prepareRelayCode()
			}
			.setNegativeButton(R.string.action_cancel, null)
			.show()
	}

	private fun showManualTokenDialog()
	{
		val dialogView = layoutInflater.inflate(R.layout.dialog_manual_npsso, null)
		val tokenInput = dialogView.findViewById<TextInputEditText>(R.id.npssoTokenInput)
		val btnLogin = dialogView.findViewById<Button>(R.id.btnOpenPlayStationLogin)
		val btnNpsso = dialogView.findViewById<Button>(R.id.btnOpenNpssoPage)
		val btnPaste = dialogView.findViewById<Button>(R.id.btnPasteClipboard)

		val psnLoginUrl = buildSonyOAuthUrl()

		btnLogin.setOnClickListener {
			try {
				val intent = Intent(Intent.ACTION_VIEW, Uri.parse(psnLoginUrl))
				startActivity(intent)
			} catch (e: Exception) {
				Log.e(TAG, "Failed to open PSN login URL", e)
				try {
					startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://my.playstation.com")))
				} catch (_: Exception) {
					Toast.makeText(this, R.string.psn_login_external_browser_error, Toast.LENGTH_SHORT).show()
				}
			}
		}

		btnNpsso.setOnClickListener {
			try {
				val intent = Intent(Intent.ACTION_VIEW, Uri.parse(PsnAuthConstants.SSOCOOKIE_ENDPOINT))
				startActivity(intent)
			} catch (e: Exception) {
				Log.e(TAG, "Failed to open SSO cookie endpoint", e)
				Toast.makeText(this, R.string.psn_login_external_browser_error, Toast.LENGTH_SHORT).show()
			}
		}

		btnPaste.setOnClickListener {
			val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager
			val clip = clipboard?.primaryClip?.getItemAt(0)?.text?.toString()?.trim()
			if (!clip.isNullOrEmpty()) {
				tokenInput.setText(clip)
				tokenInput.setSelection(tokenInput.text?.length ?: 0)
			} else {
				Toast.makeText(this, "Clipboard is empty", Toast.LENGTH_SHORT).show()
			}
		}

		MaterialAlertDialogBuilder(this)
			.setTitle(R.string.psn_login_npsso_dialog_title)
			.setView(dialogView)
			.setPositiveButton(R.string.psn_login_connect_button) { _, _ ->
				val rawToken = tokenInput.text?.toString()?.trim() ?: ""
				val token = sanitizeNpsso(rawToken)
				if (token.length != 64) {
					Toast.makeText(this, R.string.psn_login_invalid_npsso, Toast.LENGTH_LONG).show()
					return@setPositiveButton
				}
				onLoginSuccess(token)
			}
			.setNegativeButton(R.string.action_cancel, null)
			.show()
	}

	private fun sanitizeNpsso(input: String): String
	{
		val trimmed = input.trim()
		val tokenRegex = Regex("[a-zA-Z0-9_-]{64}")
		val match = tokenRegex.find(trimmed)
		if (match != null) {
			return match.value
		}
		if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
			try {
				val json = JSONObject(trimmed)
				val token = json.optString("npsso")
				if (token.isNotEmpty()) return token.trim()
			} catch (_: Exception) {}
		}
		return trimmed.removeSurrounding("\"").trim()
	}

	private suspend fun createAstraPlayCode(code: String): Boolean = withContext(Dispatchers.IO)
	{
		if (embeddedRelayServer != null && preferences.psnRelayUrl.isEmpty()) {
			return@withContext embeddedRelayServer!!.createCode(code)
		}

		try
		{
			val url = URL("${relayBaseUrl()}/psstream/create-code")
			val connection = url.openConnection() as HttpURLConnection
			connection.connectTimeout = 5000
			connection.readTimeout = 5000
			connection.requestMethod = "POST"
			connection.setRequestProperty("Content-Type", "application/json")
			connection.doOutput = true

			val jsonPayload = JSONObject().apply {
				put("code", code)
			}.toString()

			connection.outputStream.use { it.write(jsonPayload.toByteArray()) }

			val responseCode = connection.responseCode
			if (responseCode == HttpURLConnection.HTTP_OK)
			{
				val response = connection.inputStream.bufferedReader().use { it.readText() }
				val jsonResponse = JSONObject(response)
				return@withContext jsonResponse.optString("result") == "success"
			}
			return@withContext false
		}
		catch (e: Exception)
		{
			Log.w(TAG, "Exception creating astraplay code on remote relay", e)
			return@withContext false
		}
	}

	private fun checkTokenStatus(manual: Boolean = false)
	{
		checkStatusButton.isEnabled = false
		progressBar.visibility = View.VISIBLE
		statusTextView.text = getString(R.string.psn_login_checking_status)

		scope.launch {
			val token = checkAstraPlayStatus(loginCode)
			progressBar.visibility = View.GONE
			checkStatusButton.isEnabled = true

			if (token != null) {
				Log.i(TAG, "NPSSO token received from relay server")
				onLoginSuccess(token)
			} else {
				val msg = getString(R.string.psn_login_not_complete)
				statusTextView.text = msg
				if (manual) {
					Toast.makeText(this@PsnLoginActivity, R.string.psn_login_not_complete_toast, Toast.LENGTH_SHORT).show()
				}
			}
		}
	}

	private suspend fun checkAstraPlayStatus(code: String): String? = withContext(Dispatchers.IO)
	{
		embeddedRelayServer?.getToken(code)?.let { return@withContext it }

		try
		{
			val url = URL("${relayBaseUrl()}/psstream/get-tokens")
			val connection = url.openConnection() as HttpURLConnection
			connection.connectTimeout = 4000
			connection.readTimeout = 4000
			connection.requestMethod = "POST"
			connection.setRequestProperty("Content-Type", "application/json")
			connection.doOutput = true

			val jsonPayload = JSONObject().apply {
				put("code", code)
			}.toString()

			connection.outputStream.use { it.write(jsonPayload.toByteArray()) }

			val responseCode = connection.responseCode
			if (responseCode == HttpURLConnection.HTTP_OK)
			{
				val response = connection.inputStream.bufferedReader().use { it.readText() }
				val jsonResponse = JSONObject(response)
				val result = jsonResponse.optString("result")
				if (result == "success") {
					val npsso = jsonResponse.optString("npsso")
					if (npsso.isNotEmpty()) return@withContext npsso
				}
			}
			return@withContext null
		}
		catch (e: Exception)
		{
			Log.d(TAG, "Exception checking token status: ${e.message}")
			return@withContext null
		}
	}

	private fun onLoginSuccess(token: String)
	{
		if (!isLoginHandled.compareAndSet(false, true)) {
			Log.d(TAG, "onLoginSuccess already handled; ignoring duplicate call")
			return
		}

		autoPollJob?.cancel()

		// Show feedback in both views
		showWebViewOverlay("Validating PlayStation account...")
		progressBar.visibility = View.VISIBLE
		statusTextView.text = getString(R.string.psn_login_connecting)
		Toast.makeText(this, "Validating token with PlayStation Network...", Toast.LENGTH_SHORT).show()

		Thread {
			val prefs = Preferences(this)
			val psnTokenManager = PsnTokenManager(prefs)
			val exchangeSuccess = psnTokenManager.exchangeNpssoForTokens(token)

			runOnUiThread {
				progressBar.visibility = View.GONE
				if (exchangeSuccess) {
					Log.i(TAG, "PSN login complete: NPSSO + Remote Play tokens saved")
					tokenManager.saveNpssoToken(token)

					val resultIntent = Intent().apply {
						putExtra(EXTRA_NPSSO_TOKEN, token)
					}
					setResult(RESULT_LOGIN_SUCCESS, resultIntent)
					Toast.makeText(this, getString(R.string.psn_login_success), Toast.LENGTH_SHORT).show()
					finish()
				} else {
					Log.w(TAG, "PSN login: Remote Play token exchange failed for token")
					isLoginHandled.set(false)
					tokenManager.clearNpssoToken()
					webViewLoadingOverlay.visibility = View.GONE
					statusTextView.text = "Token exchange failed. Token may be expired or invalid. Please try again."
					Toast.makeText(this, "Token exchange failed. Please sign in again to get a fresh token.", Toast.LENGTH_LONG).show()
					checkStatusButton.isEnabled = true
					openBrowserButton.isEnabled = true
					manualTokenButton.isEnabled = true
				}
			}
		}.start()
	}

	override fun onBackPressed()
	{
		if (layoutWebView.visibility == View.VISIBLE && loginWebView.canGoBack()) {
			loginWebView.goBack()
		} else {
			setResult(RESULT_LOGIN_CANCELLED)
			super.onBackPressed()
		}
	}

	override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean
	{
		when (keyCode) {
			KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_R1 -> {
				if (layoutWebView.visibility == View.VISIBLE) {
					showPhoneRelayView()
				} else {
					showWebView()
				}
				return true
			}
			KeyEvent.KEYCODE_BUTTON_B -> {
				setResult(RESULT_LOGIN_CANCELLED)
				finish()
				return true
			}
		}
		return super.onKeyDown(keyCode, event)
	}

	override fun onDestroy()
	{
		super.onDestroy()
		autoPollJob?.cancel()
		scope.cancel()
		loginWebView.stopLoading()
		loginWebView.destroy()
		EmbeddedRelayServer.stop()
	}
}
