// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.xbox

import android.content.Context
import android.util.Log
import com.metallic.chiaki.common.Preferences
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

object XboxAuthManager {
    private const val TAG = "XboxAuthManager"
    private const val MSAL_CLIENT_ID = "1f907974-e22b-4810-a9de-d9647380c97e"
    private const val MSAL_SCOPE = "xboxlive.signin openid profile offline_access"

    var isPolling: Boolean = false
        private set

    fun cancelLogin() {
        isPolling = false
    }

    suspend fun startDeviceCodeLogin(
        context: Context,
        onDeviceCode: (userCode: String, verificationUri: String, expiresIn: Int) -> Unit,
        onSuccess: (gamertag: String) -> Unit,
        onError: (message: String) -> Unit
    ) = withContext(Dispatchers.IO) {
        try {
            isPolling = true
            val url = URL("https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode")
            val conn = (url.openConnection() as HttpURLConnection).apply {
                requestMethod = "POST"
                doOutput = true
                connectTimeout = 15000
                readTimeout = 15000
                setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
            }

            val body = "client_id=" + URLEncoder.encode(MSAL_CLIENT_ID, "UTF-8") +
                    "&scope=" + URLEncoder.encode(MSAL_SCOPE, "UTF-8")
            OutputStreamWriter(conn.outputStream).use { it.write(body); it.flush() }

            if (conn.responseCode != 200) {
                withContext(Dispatchers.Main) { onError("Failed to request device code (${conn.responseCode})") }
                return@withContext
            }

            val resp = BufferedReader(InputStreamReader(conn.inputStream)).use { it.readText() }
            val json = JSONObject(resp)
            val userCode = json.getString("user_code")
            val deviceCode = json.getString("device_code")
            val verificationUri = json.optString("verification_uri", "https://www.microsoft.com/link")
            val expiresIn = json.optInt("expires_in", 900)
            val interval = json.optInt("interval", 5)

            withContext(Dispatchers.Main) {
                onDeviceCode(userCode, verificationUri, expiresIn)
            }

            // Polling loop
            val endTime = System.currentTimeMillis() + (expiresIn * 1000L)
            var currentInterval = interval
            while (isPolling && System.currentTimeMillis() < endTime) {
                kotlinx.coroutines.delay(currentInterval * 1000L)
                if (!isPolling) break

                val tokenUrl = URL("https://login.microsoftonline.com/consumers/oauth2/v2.0/token")
                val tokenConn = (tokenUrl.openConnection() as HttpURLConnection).apply {
                    requestMethod = "POST"
                    doOutput = true
                    connectTimeout = 15000
                    readTimeout = 15000
                    setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
                }

                val tokenBody = "grant_type=" + URLEncoder.encode("urn:ietf:params:oauth:grant-type:device_code", "UTF-8") +
                        "&client_id=" + URLEncoder.encode(MSAL_CLIENT_ID, "UTF-8") +
                        "&device_code=" + URLEncoder.encode(deviceCode, "UTF-8")
                OutputStreamWriter(tokenConn.outputStream).use { it.write(tokenBody); it.flush() }

                val code = tokenConn.responseCode
                if (code == 200) {
                    val tokenResp = BufferedReader(InputStreamReader(tokenConn.inputStream)).use { it.readText() }
                    val tokenJson = JSONObject(tokenResp)
                    val accessToken = tokenJson.getString("access_token")
                    val refreshToken = tokenJson.optString("refresh_token")

                    val preferences = Preferences(context)
                    preferences.setXboxRefreshToken(refreshToken)

                    // Exchange for XBL user token to get Gamertag
                    val gamertag = exchangeXblUserToken(context, accessToken)
                    isPolling = false
                    withContext(Dispatchers.Main) {
                        onSuccess(gamertag)
                    }
                    return@withContext
                } else {
                    val errStream = tokenConn.errorStream ?: tokenConn.inputStream
                    val errResp = BufferedReader(InputStreamReader(errStream)).use { it.readText() }
                    val errJson = JSONObject(errResp)
                    val err = errJson.optString("error")
                    when (err) {
                        "authorization_pending" -> { /* Keep polling */ }
                        "slow_down" -> { currentInterval += 5 }
                        "expired_token" -> {
                            isPolling = false
                            withContext(Dispatchers.Main) { onError("Login expired. Please try again.") }
                            return@withContext
                        }
                        else -> {
                            isPolling = false
                            withContext(Dispatchers.Main) { onError(errJson.optString("error_description", err)) }
                            return@withContext
                        }
                    }
                }
            }

            if (isPolling) {
                isPolling = false
                withContext(Dispatchers.Main) { onError("Device code login timed out.") }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Device code error", e)
            withContext(Dispatchers.Main) { onError("Network error: ${e.message}") }
        }
    }

    private fun exchangeXblUserToken(context: Context, accessToken: String): String {
        try {
            val url = URL("https://user.auth.xboxlive.com/user/authenticate")
            val conn = (url.openConnection() as HttpURLConnection).apply {
                requestMethod = "POST"
                doOutput = true
                connectTimeout = 15000
                readTimeout = 15000
                setRequestProperty("Content-Type", "application/json")
                setRequestProperty("x-xbl-contract-version", "1")
                setRequestProperty("Accept", "application/json")
            }

            val payload = JSONObject().apply {
                put("Properties", JSONObject().apply {
                    put("AuthMethod", "RPS")
                    put("RpsTicket", "d=$accessToken")
                    put("SiteName", "user.auth.xboxlive.com")
                })
                put("RelyingParty", "http://auth.xboxlive.com")
                put("TokenType", "JWT")
            }

            OutputStreamWriter(conn.outputStream).use { it.write(payload.toString()); it.flush() }
            if (conn.responseCode == 200) {
                val resp = BufferedReader(InputStreamReader(conn.inputStream)).use { it.readText() }
                val json = JSONObject(resp)
                val displayClaims = json.optJSONObject("DisplayClaims")
                val xui = displayClaims?.optJSONArray("xui")
                if (xui != null && xui.length() > 0) {
                    val claim = xui.getJSONObject(0)
                    val gamertag = claim.optString("gtg", "Xbox User")
                    val uhs = claim.optString("uhs", "")
                    val preferences = Preferences(context)
                    preferences.setXboxGamertag(gamertag)
                    preferences.setXboxUserHash(uhs)
                    return gamertag
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "exchangeXblUserToken error", e)
        }
        val defaultGt = "Xbox Player"
        Preferences(context).setXboxGamertag(defaultGt)
        return defaultGt
    }
}
