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
import android.view.Window
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.webkit.CookieManager
import android.webkit.PermissionRequest
import android.webkit.WebChromeClient
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.activity.OnBackPressedCallback
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
    private val handler = Handler(Looper.getMainLooper())

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 1. Enforce True Fullscreen Window
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        )
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        binding = ActivityXboxStreamBinding.inflate(layoutInflater)
        setContentView(binding.root)

        enableImmersiveFullscreen()

        val title = intent.getStringExtra(EXTRA_TITLE) ?: "Xbox Cloud Gaming"
        val streamUrl = intent.getStringExtra(EXTRA_URL) ?: "https://www.xbox.com/play"

        binding.overlayGameTitle.text = title

        // 2. Direct, Reliable Exit Stream Button
        binding.stopStreamButton.setOnClickListener {
            stopSessionAndExit()
        }

        // Auto-fade title pill after 4 seconds
        handler.postDelayed({
            binding.streamStatusPill.animate()
                .alpha(0f)
                .setDuration(500)
                .withEndAction { binding.streamStatusPill.visibility = View.GONE }
                .start()
        }, 4000)

        // 3. Handle Back Key
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                stopSessionAndExit()
            }
        })

        setupWebView(streamUrl)
    }

    override fun onResume() {
        super.onResume()
        enableImmersiveFullscreen()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            enableImmersiveFullscreen()
        }
    }

    private fun enableImmersiveFullscreen() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let { controller ->
                controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                controller.systemBarsBehavior =
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        }
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
                if (newProgress >= 50 && view != null) {
                    injectAutoPlayScript(view)
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
                if (view != null) {
                    injectAutoPlayScript(view)
                }
            }
        }

        webView.loadUrl(url)
    }

    private fun injectAutoPlayScript(webView: WebView) {
        val js = """
            (function() {
                // 1. Inject styling to hide web navigation headers/footers for a clean native console look
                if (!document.getElementById('astraplay-custom-style')) {
                    const style = document.createElement('style');
                    style.id = 'astraplay-custom-style';
                    style.innerHTML = 'header, #uhfHeader, .uhf-header, #uhfFooter, .c-uhf-footer { display: none !important; }';
                    (document.head || document.documentElement).appendChild(style);
                }

                if (window._astraPlayAutoPlayObserver) return;
                window._astraPlayAutoPlayObserver = true;

                let attempts = 0;
                const maxAttempts = 80; // 24 seconds (300ms intervals)

                const timer = setInterval(function() {
                    attempts++;
                    if (attempts > maxAttempts) {
                        clearInterval(timer);
                        return;
                    }

                    // A. Check for modal warning dialogs (e.g. "Continue anyway", "Play anyway")
                    const allButtons = Array.from(document.querySelectorAll('button, a, div[role="button"]'));
                    for (const btn of allButtons) {
                        const txt = (btn.innerText || btn.textContent || '').trim().toLowerCase();
                        if (txt === 'continue anyway' || txt === 'play anyway' || txt === 'play with touch') {
                            console.log('[AstraPlay] Auto-clicking confirmation modal:', txt);
                            btn.click();
                            return;
                        }
                    }

                    // B. Auto-click primary "Play with cloud" / "Play" CTA
                    for (const el of allButtons) {
                        const txt = (el.innerText || el.textContent || '').trim().toLowerCase();
                        const testId = (el.getAttribute('data-testid') || '').toLowerCase();
                        const aria = (el.getAttribute('aria-label') || '').toLowerCase();

                        const isPlayText = txt.startsWith('play with cloud') ||
                                           txt.startsWith('play with game pass') ||
                                           txt === 'play';
                        const isPlayAttr = testId.includes('play-button') || aria.includes('play with cloud') || aria === 'play';

                        if ((isPlayText || isPlayAttr) && !el.disabled && el.offsetParent !== null) {
                            console.log('[AstraPlay] Found Play button, auto-clicking:', txt || testId || aria);
                            el.click();
                            clearInterval(timer);
                            break;
                        }
                    }
                }, 300);
            })();
        """.trimIndent()
        webView.evaluateJavascript(js, null)
    }

    private fun stopSessionAndExit() {
        binding.xboxWebView.apply {
            stopLoading()
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
                KeyEvent.KEYCODE_ESCAPE,
                KeyEvent.KEYCODE_GUIDE -> {
                    stopSessionAndExit()
                    return true
                }
            }
        }
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        return super.dispatchGenericMotionEvent(ev)
    }

    override fun onDestroy() {
        handler.removeCallbacksAndMessages(null)
        binding.xboxWebView.apply {
            stopLoading()
            loadUrl("about:blank")
            destroy()
        }
        super.onDestroy()
    }
}
