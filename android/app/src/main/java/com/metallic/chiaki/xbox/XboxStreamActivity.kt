// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.xbox

import android.annotation.SuppressLint
import android.graphics.Bitmap
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.webkit.CookieManager
import android.webkit.PermissionRequest
import android.webkit.WebChromeClient
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import io.github.nileshchakraborty.astraplay.databinding.ActivityXboxStreamBinding

class XboxStreamActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_URL = "extra_stream_url"
        const val EXTRA_TITLE = "extra_stream_title"
        const val EXTRA_TITLE_ID = "extra_title_id"
        private const val DESKTOP_USER_AGENT =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36 Edg/130.0.0.0"
    }

    private lateinit var binding: ActivityXboxStreamBinding
    private val hideHandler = Handler(Looper.getMainLooper())
    private val hideOverlayRunnable = Runnable {
        binding.overlayBar.animate().alpha(0f).setDuration(400).withEndAction {
            binding.overlayBar.visibility = View.GONE
        }.start()
    }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityXboxStreamBinding.inflate(layoutInflater)
        setContentView(binding.root)

        enableImmersiveMode()

        val title = intent.getStringExtra(EXTRA_TITLE) ?: "Xbox Cloud Gaming"
        val streamUrl = intent.getStringExtra(EXTRA_URL) ?: "https://www.xbox.com/play"

        binding.overlayGameTitle.text = title

        binding.stopStreamButton.setOnClickListener {
            stopSessionAndExit()
        }

        binding.root.setOnClickListener {
            showOverlay()
        }

        setupWebView(streamUrl)
        scheduleHideOverlay(5000)
    }

    private fun enableImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.let { controller ->
                controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                controller.systemBarsBehavior =
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_FULLSCREEN
            )
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    private fun setupWebView(url: String) {
        val webView = binding.xboxWebView

        val cookieManager = CookieManager.getInstance()
        cookieManager.setAcceptCookie(true)
        cookieManager.setAcceptThirdPartyCookies(webView, true)

        webView.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            databaseEnabled = true
            mediaPlaybackRequiresUserGesture = false
            useWideViewPort = true
            loadWithOverviewMode = true
            userAgentString = DESKTOP_USER_AGENT
            mixedContentMode = WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE
            cacheMode = WebSettings.LOAD_DEFAULT
        }

        webView.webChromeClient = object : WebChromeClient() {
            override fun onPermissionRequest(request: PermissionRequest) {
                request.grant(request.resources)
            }

            override fun onProgressChanged(view: WebView?, newProgress: Int) {
                binding.loadingProgressBar.progress = newProgress
                if (newProgress >= 100) {
                    binding.loadingProgressBar.visibility = View.GONE
                } else {
                    binding.loadingProgressBar.visibility = View.VISIBLE
                }
            }
        }

        webView.webViewClient = object : WebViewClient() {
            override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
                super.onPageStarted(view, url, favicon)
                binding.loadingProgressBar.visibility = View.VISIBLE
            }

            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
                binding.loadingProgressBar.visibility = View.GONE
            }
        }

        webView.loadUrl(url)
    }

    private fun scheduleHideOverlay(delayMs: Long) {
        hideHandler.removeCallbacks(hideOverlayRunnable)
        hideHandler.postDelayed(hideOverlayRunnable, delayMs)
    }

    private fun showOverlay() {
        binding.overlayBar.visibility = View.VISIBLE
        binding.overlayBar.alpha = 1f
        scheduleHideOverlay(4000)
    }

    private fun confirmStopSession() {
        AlertDialog.Builder(this)
            .setTitle("Stop Xbox Stream")
            .setMessage("Do you want to stop and exit the Xbox Cloud Gaming session?")
            .setPositiveButton("Stop Session") { _, _ ->
                stopSessionAndExit()
            }
            .setNegativeButton("Continue Playing", null)
            .show()
    }

    private fun stopSessionAndExit() {
        binding.xboxWebView.apply {
            loadUrl("about:blank")
            clearHistory()
            removeAllViews()
            destroy()
        }
        finish()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_DOWN) {
            when (event.keyCode) {
                KeyEvent.KEYCODE_BACK,
                KeyEvent.KEYCODE_BUTTON_B -> {
                    if (binding.overlayBar.visibility == View.VISIBLE) {
                        stopSessionAndExit()
                        return true
                    } else {
                        showOverlay()
                        return true
                    }
                }
                KeyEvent.KEYCODE_BUTTON_START,
                KeyEvent.KEYCODE_MENU -> {
                    showOverlay()
                }
            }
        }
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        return super.dispatchGenericMotionEvent(ev)
    }

    override fun onDestroy() {
        hideHandler.removeCallbacks(hideOverlayRunnable)
        binding.xboxWebView.apply {
            stopLoading()
            loadUrl("about:blank")
            destroy()
        }
        super.onDestroy()
    }
}
