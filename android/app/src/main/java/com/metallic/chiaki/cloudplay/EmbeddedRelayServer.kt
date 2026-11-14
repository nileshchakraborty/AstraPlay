// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.cloudplay

import android.content.Context
import android.util.Log
import org.json.JSONObject
import java.io.*
import java.net.*
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Lightweight in-app embedded HTTP relay server for PSN login.
 * Mirrors desktop AstraPlay's EmbeddedRelayServer (gui/src/embeddedrelayserver.cpp).
 *
 * Runs locally on port 25525 (with automatic fallback to port 0 if 25525 is occupied)
 * and serves the mobile 3-step auth portal on the local Wi-Fi network.
 * Allows mobile phones to scan the tablet's QR code and complete PSN sign-in
 * without requiring any external or self-hosted relay infrastructure.
 */
class EmbeddedRelayServer private constructor(private val appContext: Context) {

	companion object {
		private const val TAG = "EmbeddedRelayServer"
		const val DEFAULT_PORT = 25525
		private const val CODE_TTL_MS = 15L * 60L * 1000L // 15 minutes

		@Volatile
		private var instance: EmbeddedRelayServer? = null

		@Synchronized
		fun getInstance(): EmbeddedRelayServer? = instance

		@Synchronized
		fun start(context: Context): EmbeddedRelayServer {
			instance?.let { existing ->
				if (existing.isRunning) {
					return existing
				}
				existing.stop()
			}
			val server = EmbeddedRelayServer(context.applicationContext)
			server.start()
			instance = server
			return server
		}

		@Synchronized
		fun stop() {
			instance?.stop()
			instance = null
		}

		/**
		 * Identifies virtual, container, and emulator NAT IPv4 subnets that cannot be reached
		 * by external devices on a physical Wi-Fi LAN.
		 */
		fun isVirtualSubnet(host: String): Boolean {
			return host.startsWith("10.0.2.") ||       // Android emulator / QEMU NAT
				host.startsWith("10.0.3.") ||       // BlueStacks / Genymotion NAT
				host.startsWith("192.168.56.") ||   // VirtualBox Host-Only default
				host.startsWith("192.168.232.") ||  // BlueStacks alternate NAT
				host.startsWith("192.168.240.") ||  // Waydroid default bridge
				host.startsWith("192.168.245.") ||  // Waydroid alternate bridge
				host.startsWith("192.168.250.") ||  // Waydroid alternate bridge
				host.startsWith("192.168.122.") ||  // KVM / libvirt default bridge
				host.startsWith("172.17.") ||       // Docker default bridge
				host.startsWith("172.18.") ||       // Docker default bridge
				host.startsWith("172.19.")          // Docker default bridge
		}

		/**
		 * Identifies virtual, bridge, container, or tunnel network interfaces.
		 */
		fun isVirtualInterface(name: String): Boolean {
			val lower = name.lowercase()
			return lower.contains("dummy") || lower.contains("tun") || lower.contains("tap") ||
				lower.contains("p2p") || lower.contains("waydroid") || lower.contains("veth") ||
				lower.contains("docker") || lower.contains("virbr") || lower.contains("vboxnet") ||
				lower.contains("virtualbox") || lower.contains("vbox") || lower.contains("bluestacks") ||
				lower.contains("bstk") || lower.contains("nox") || lower.contains("ldplayer") ||
				lower.contains("memu") || lower.contains("vethernet") || lower.contains("hyper-v") ||
				lower.contains("wsl") || lower.contains("lxc") || lower.contains("anbox") ||
				lower.contains("vmnet") || lower.contains("vmware") || lower.contains("tailscale") ||
				lower.contains("wg") || lower.contains("bridge")
		}

		/**
		 * Detects whether Android is running inside an emulator or container
		 * (Waydroid, BlueStacks, LDPlayer, Nox, Genymotion, QEMU, WSA, etc.).
		 */
		fun isEmulatorOrContainer(): Boolean {
			return try {
				if (isWaydroid()) return true

				val buildProps = listOf(
					android.os.Build.HARDWARE,
					android.os.Build.PRODUCT,
					android.os.Build.BOARD,
					android.os.Build.BOOTLOADER,
					android.os.Build.MANUFACTURER,
					android.os.Build.BRAND,
					android.os.Build.MODEL,
					android.os.Build.DEVICE,
					android.os.Build.FINGERPRINT
				).joinToString(" ").lowercase()

				if (buildProps.contains("bluestacks") ||
					buildProps.contains("nox") ||
					buildProps.contains("ldplayer") ||
					buildProps.contains("vbox86") ||
					buildProps.contains("goldfish") ||
					buildProps.contains("ranchu") ||
					buildProps.contains("genymotion") ||
					buildProps.contains("google_sdk") ||
					buildProps.contains("android sdk built for") ||
					buildProps.contains("emulator") ||
					buildProps.contains("subsystem for android") ||
					buildProps.contains("ttvm") ||
					buildProps.contains("andy")
				) return true

				val qemu = getSystemPropertyStatic("ro.kernel.qemu")
				if (qemu == "1") return true

				false
			} catch (_: Throwable) {
				false
			}
		}

		/**
		 * Detects whether Android is running inside a Waydroid container on Linux.
		 */
		fun isWaydroid(): Boolean {
			return try {
				val buildProps = listOf(
					android.os.Build.HARDWARE,
					android.os.Build.PRODUCT,
					android.os.Build.BOARD,
					android.os.Build.MANUFACTURER,
					android.os.Build.BRAND,
					android.os.Build.MODEL,
					android.os.Build.DEVICE
				).joinToString(" ").lowercase()
				if (buildProps.contains("waydroid")) return true

				val bootHardware = getSystemPropertyStatic("ro.boot.hardware")?.lowercase() ?: ""
				if (bootHardware.contains("waydroid")) return true
				val hardware = getSystemPropertyStatic("ro.hardware")?.lowercase() ?: ""
				if (hardware.contains("waydroid")) return true

				false
			} catch (_: Throwable) {
				false
			}
		}

		fun getSystemPropertyStatic(key: String): String? {
			return try {
				val clazz = Class.forName("android.os.SystemProperties")
				val method = clazz.getMethod("get", String::class.java)
				val value = method.invoke(null, key) as? String
				value?.trim()?.takeIf { it.isNotEmpty() }
			} catch (_: Throwable) {
				null
			}
		}

		/** Accept only a literal, usable IPv4 address for the phone-facing host override. */
		fun isValidLanIpOverride(value: String): Boolean {
			val octets = value.split('.')
			return octets.size == 4 && octets.all { it.isNotEmpty() && it.all(Char::isDigit) && (it.toIntOrNull() ?: 256) in 0..255 } &&
				!value.startsWith("0.") && !value.startsWith("127.") && !isVirtualSubnet(value)
		}

		fun selectLanIpOverride(getProperty: (String) -> String?): String? =
			listOf("debug.astraplay.lan_ip", "persist.astraplay.lan_ip", "persist.waydroid.host_ip")
				.mapNotNull(getProperty)
				.firstOrNull { isValidLanIpOverride(it) }

		/**
		 * Reads the default IPv4 gateway from /proc/net/route (Linux only).
		 * Works universally inside any Linux-based container or emulator (Waydroid, QEMU,
		 * BlueStacks on Linux, WSA, etc.) without host-side daemon or property injection.
		 *
		 * Strategy:
		 *  1. Standard: look for a default route (Destination=00000000) with RTF_GATEWAY flag.
		 *  2. Waydroid fallback: Waydroid's Android kernel namespace has no explicit default
		 *     gateway entry — only a directly-connected subnet route (RTF_UP only). In this case
		 *     we infer the host gateway as the first host address (.1) of that subnet.
		 *
		 * Returns the gateway IP as dotted-decimal, or null if unavailable.
		 */
		fun readDefaultGatewayIp(): String? {
			return try {
				val lines = java.io.BufferedReader(java.io.FileReader("/proc/net/route")).readLines()
				var inferredSubnetGateway: String? = null

				for (line in lines.drop(1)) {
					val parts = line.trim().split("\\s+".toRegex())
					if (parts.size < 8) continue
					val destination = parts[1]
					val gateway     = parts[2]
					val flags       = parts[3].toIntOrNull(16) ?: continue
					val mask        = parts[7]

					// 1. Standard default route: Dest=0.0.0.0, RTF_UP | RTF_GATEWAY
					if (destination == "00000000" && (flags and 0x3) == 0x3) {
						val gwLong = gateway.toLongOrNull(16) ?: continue
						val gwIp = longToIp(gwLong)
						if (!gwIp.startsWith("0.") && !gwIp.startsWith("127.")) {
							Log.i(TAG, "Default gateway from /proc/net/route: $gwIp")
							return gwIp
						}
					}

					// 2. Waydroid fallback: directly-connected subnet (RTF_UP only, no RTF_GATEWAY,
					// gateway field is 00000000). Infer host gateway as subnet_base | 1 (e.g. .1).
					if (inferredSubnetGateway == null &&
						(flags and 0x3) == 0x1 && gateway == "00000000" &&
						destination != "00000000"
					) {
						val destLong = destination.toLongOrNull(16) ?: continue
						val maskLong = mask.toLongOrNull(16) ?: continue
						val subnetBase = destLong and maskLong
						// Host byte-order first host in subnet = subnet_base | 1
						val gwLong = subnetBase or 0x01000000L  // set byte0 (LE) = 1 → host .1
						val gwIp = longToIp(gwLong)
						if (!gwIp.startsWith("0.") && !gwIp.startsWith("127.")) {
							inferredSubnetGateway = gwIp
							Log.i(TAG, "Inferred host gateway from directly-connected subnet: $gwIp")
						}
					}
				}
				inferredSubnetGateway
			} catch (_: Throwable) {
				null
			}
		}

		/** Converts a little-endian 32-bit long from /proc/net/route to dotted-decimal IPv4. */
		private fun longToIp(le: Long): String {
			val b0 = (le and 0xFF).toInt()
			val b1 = ((le shr 8) and 0xFF).toInt()
			val b2 = ((le shr 16) and 0xFF).toInt()
			val b3 = ((le shr 24) and 0xFF).toInt()
			return "$b0.$b1.$b2.$b3"
		}

		/**
		 * Probes whether an AstraPlay relay server is reachable at the host gateway IP on
		 * DEFAULT_PORT. Allows users who have set up a host-side relay proxy (socat, Nginx, etc.)
		 * forwarding port 25525 to get transparent QR login with no manual IP configuration.
		 * Uses a 1500ms TCP connect-only probe (no data sent).
		 * MUST be called from a background (IO) thread.
		 * Returns the gateway IP string if relay is reachable, null otherwise.
		 */
		fun probeRelayAtGateway(timeoutMs: Int = 1500): String? {
			val gatewayIp = readDefaultGatewayIp() ?: return null
			return try {
				Socket().use { socket ->
					socket.connect(InetSocketAddress(gatewayIp, DEFAULT_PORT), timeoutMs)
					Log.i(TAG, "Host relay reachable at gateway $gatewayIp:$DEFAULT_PORT")
					gatewayIp
				}
			} catch (_: Throwable) {
				Log.d(TAG, "No host relay at gateway $gatewayIp:$DEFAULT_PORT")
				null
			}
		}
	}

	data class CodeEntry(val createdAt: Long, var npsso: String? = null)

	private val codeStore = ConcurrentHashMap<String, CodeEntry>()
	private val running = AtomicBoolean(false)
	private var serverSocket: ServerSocket? = null
	private var executor = Executors.newCachedThreadPool()
	private var boundPort = DEFAULT_PORT
	@Volatile
	private var cachedLanIp: String? = null

	val isRunning: Boolean get() = running.get()
	val port: Int get() = boundPort

	// In-memory cache for static assets from assets/relay/
	private var htmlBytes: ByteArray? = null
	private var cssBytes: ByteArray? = null
	private var jsBytes: ByteArray? = null

	private fun loadAssets() {
		try {
			appContext.assets.open("relay/index.html").use { htmlBytes = it.readBytes() }
		} catch (e: Exception) {
			Log.e(TAG, "Failed to load relay/index.html from assets", e)
		}
		try {
			appContext.assets.open("relay/styles.css").use { cssBytes = it.readBytes() }
		} catch (e: Exception) {
			Log.e(TAG, "Failed to load relay/styles.css from assets", e)
		}
		try {
			appContext.assets.open("relay/script.js").use { jsBytes = it.readBytes() }
		} catch (e: Exception) {
			Log.e(TAG, "Failed to load relay/script.js from assets", e)
		}
	}

	fun start() {
		if (running.get()) return

		loadAssets()

		try {
			// Try preferred port 25525 first
			serverSocket = try {
				ServerSocket(DEFAULT_PORT, 50, InetAddress.getByName("0.0.0.0"))
			} catch (e: Exception) {
				Log.w(TAG, "Could not bind port $DEFAULT_PORT, falling back to random available port", e)
				ServerSocket(0, 50, InetAddress.getByName("0.0.0.0"))
			}
			boundPort = serverSocket?.localPort ?: DEFAULT_PORT
			running.set(true)
			executor = Executors.newCachedThreadPool()

			val acceptThread = Thread({
				try {
					getLanIpAddress(forceRefresh = true)
				} catch (_: Throwable) {}
				Log.i(TAG, "EmbeddedRelayServer listening on port $boundPort (URL: ${serverUrl()})")
				while (running.get()) {
					try {
						val client = serverSocket?.accept() ?: break
						executor.submit { handleConnection(client) }
					} catch (e: SocketException) {
						if (!running.get()) break
						Log.w(TAG, "ServerSocket accept exception", e)
					} catch (e: Exception) {
						Log.e(TAG, "Unexpected error in accept loop", e)
					}
				}
			}, "EmbeddedRelayServer-Accept")
			acceptThread.isDaemon = true
			acceptThread.start()
		} catch (e: Exception) {
			Log.e(TAG, "Failed to start EmbeddedRelayServer", e)
			running.set(false)
		}
	}

	fun stop() {
		running.set(false)
		try {
			serverSocket?.close()
		} catch (e: Exception) {
			Log.w(TAG, "Error closing server socket", e)
		}
		serverSocket = null
		executor.shutdownNow()
		codeStore.clear()
		cachedLanIp = null
		Log.i(TAG, "EmbeddedRelayServer stopped")
	}
	/**
	 * Find physical LAN IPv4 address (e.g. 192.168.1.x, 10.x.x.x, 172.x.x.x) for QR code generation.
	 * Explicitly filters out virtual/container bridges (Waydroid, QEMU, Docker, KVM, Wireguard, Tailscale)
	 * so external devices scanning the QR code on a local Wi-Fi network are not given an unreachable
	 * private container IP (such as Waydroid's 192.168.240.x / 192.168.245.x / 192.168.250.x).
	 *
	 * Results are cached in memory to avoid redundant socket queries and StrictMode warnings.
	 */
	fun getLanIpAddress(forceRefresh: Boolean = false): String? {
		if (!forceRefresh && cachedLanIp != null) {
			return cachedLanIp
		}

		// 1. Check for explicit system property override (useful for Waydroid host proxy or emulator testing)
		val override = selectLanIpOverride(::getSystemProperty)
		if (override != null) {
			Log.i(TAG, "Using LAN IP override: $override")
			cachedLanIp = override
			return override
		}

		// 2. Kernel outbound routing probe: connectionless UDP socket connect (zero network traffic).
		// Automatically resolves the true physical outbound adapter chosen by the OS routing table.
		try {
			DatagramSocket().use { socket ->
				socket.connect(InetAddress.getByName("8.8.8.8"), 10002)
				val localAddr = socket.localAddress
				if (localAddr is Inet4Address && !localAddr.isLoopbackAddress) {
					val ip = localAddr.hostAddress
					if (!ip.isNullOrEmpty() && (ip.startsWith("192.168.") || ip.startsWith("10.") || ip.startsWith("172.")) && !isVirtualSubnet(ip)) {
						cachedLanIp = ip
						return ip
					}
				}
			}
		} catch (_: Throwable) {}

		try {
			val interfaces = Collections.list(NetworkInterface.getNetworkInterfaces())
			val physicalCandidates = mutableListOf<String>()
			val otherCandidates = mutableListOf<String>()

			for (iface in interfaces) {
				if (!iface.isUp || iface.isLoopback) continue
				val name = iface.name.lowercase()
				if (isVirtualInterface(name)) continue

				val isPhysical = name.startsWith("wl") || name.startsWith("wifi") ||
					name.startsWith("eth") || name.startsWith("en") || name.startsWith("lan")

				val addrs = Collections.list(iface.inetAddresses)
				for (addr in addrs) {
					if (!addr.isLoopbackAddress && addr is Inet4Address) {
						val host = addr.hostAddress ?: continue
						// Exclude virtual/container NAT subnets (10.0.2.*, 192.168.240.*, 192.168.245.*, etc.)
						if (isVirtualSubnet(host)) continue

						if (isPhysical) {
							physicalCandidates.add(host)
						} else {
							otherCandidates.add(host)
						}
					}
				}
			}

			// Prioritize physical Wi-Fi / Ethernet adapters first
			val found = physicalCandidates.firstOrNull { it.startsWith("192.168.") }
				?: physicalCandidates.firstOrNull { it.startsWith("10.") }
				?: physicalCandidates.firstOrNull { it.startsWith("172.") }
				?: physicalCandidates.firstOrNull()
				?: otherCandidates.firstOrNull { it.startsWith("192.168.") }
				?: otherCandidates.firstOrNull { it.startsWith("10.") }
				?: otherCandidates.firstOrNull { it.startsWith("172.") }
				?: otherCandidates.firstOrNull()

			if (found != null) {
				cachedLanIp = found
				return found
			}
		} catch (t: Throwable) {
			Log.w(TAG, "Error looking up LAN IP address", t)
		}

		return null
	}

	fun hasRoutableLanIp(): Boolean = !getLanIpAddress().isNullOrEmpty()

	private fun getSystemProperty(key: String): String? = getSystemPropertyStatic(key)

	/**
	 * URL for other devices on the LAN (QR code target).
	 * If no LAN IP found, falls back to 127.0.0.1.
	 */
	fun serverUrl(): String {
		val lanIp = getLanIpAddress()
		return if (!lanIp.isNullOrEmpty()) {
			"http://$lanIp:$boundPort"
		} else {
			"http://127.0.0.1:$boundPort"
		}
	}

	/**
	 * Local URL for in-app browser opens on the same device.
	 */
	fun localUrl(): String = "http://127.0.0.1:$boundPort"

	fun createCode(code: String): Boolean {
		val norm = code.trim().uppercase()
		if (norm.isEmpty()) return false
		cleanupExpired()
		codeStore[norm] = CodeEntry(createdAt = System.currentTimeMillis())
		Log.i(TAG, "Created code: $norm (total active: ${codeStore.size})")
		return true
	}

	fun isCodeValid(code: String): Boolean {
		val norm = code.trim().uppercase()
		val entry = codeStore[norm] ?: return false
		if (System.currentTimeMillis() - entry.createdAt >= CODE_TTL_MS) {
			codeStore.remove(norm)
			return false
		}
		return true
	}

	fun setToken(code: String, npsso: String): Boolean {
		val norm = code.trim().uppercase()
		val entry = codeStore[norm] ?: return false
		if (System.currentTimeMillis() - entry.createdAt >= CODE_TTL_MS) {
			codeStore.remove(norm)
			return false
		}
		entry.npsso = npsso
		Log.i(TAG, "Token saved for code: $norm")
		return true
	}

	fun getToken(code: String): String? {
		val norm = code.trim().uppercase()
		val entry = codeStore[norm] ?: return null
		if (System.currentTimeMillis() - entry.createdAt >= CODE_TTL_MS) {
			codeStore.remove(norm)
			return null
		}
		val token = entry.npsso
		if (token != null) {
			codeStore.remove(norm)
		}
		return token
	}

	private fun cleanupExpired() {
		val now = System.currentTimeMillis()
		val it = codeStore.entries.iterator()
		while (it.hasNext()) {
			val entry = it.next()
			if (now - entry.value.createdAt >= CODE_TTL_MS) {
				it.remove()
			}
		}
	}

	private fun handleConnection(socket: Socket) {
		try {
			socket.soTimeout = 10000
			val input = BufferedInputStream(socket.getInputStream())
			val output = BufferedOutputStream(socket.getOutputStream())

			// Read request line
			val requestLine = readLine(input) ?: return
			val parts = requestLine.split(" ")
			if (parts.size < 2) return

			val method = parts[0].uppercase()
			val fullPath = parts[1]
			val path = fullPath.substringBefore("?")

			// Read headers
			var contentLength = 0
			while (true) {
				val header = readLine(input) ?: break
				if (header.isEmpty()) break
				val colon = header.indexOf(':')
				if (colon > 0) {
					val key = header.substring(0, colon).trim().lowercase()
					val value = header.substring(colon + 1).trim()
					if (key == "content-length") {
						contentLength = value.toIntOrNull() ?: 0
					}
				}
			}

			// Handle CORS preflight
			if (method == "OPTIONS") {
				sendResponse(output, 204, "No Content", "text/plain", ByteArray(0))
				return
			}

			if (method == "GET" || method == "HEAD") {
				val isHead = method == "HEAD"
				when {
					path == "/" || path == "/psstream" || path == "/psstream/" ||
					path == "/astraplay" || path == "/astraplay/" || path == "/index.html" -> {
						val bytes = htmlBytes ?: fallbackHtml().toByteArray(Charsets.UTF_8)
						sendResponse(output, 200, "OK", "text/html; charset=utf-8", if (isHead) ByteArray(0) else bytes, bytes.size)
					}
					path.endsWith("styles.css") -> {
						val bytes = cssBytes ?: ByteArray(0)
						sendResponse(output, 200, "OK", "text/css; charset=utf-8", if (isHead) ByteArray(0) else bytes, bytes.size)
					}
					path.endsWith("script.js") -> {
						val bytes = jsBytes ?: ByteArray(0)
						sendResponse(output, 200, "OK", "application/javascript; charset=utf-8", if (isHead) ByteArray(0) else bytes, bytes.size)
					}
					else -> {
						sendResponse(output, 404, "Not Found", "text/plain", ByteArray(0))
					}
				}
				return
			}

			if (method == "POST") {
				val bodyBytes = ByteArray(contentLength)
				var read = 0
				while (read < contentLength) {
					val count = input.read(bodyBytes, read, contentLength - read)
					if (count < 0) break
					read += count
				}
				val body = String(bodyBytes, Charsets.UTF_8)
				val json = try { JSONObject(body) } catch (_: Exception) { JSONObject() }

				when (path) {
					"/psstream/create-code", "/astraplay/create-code" -> {
						val code = json.optString("code")
						if (code.isNotEmpty() && createCode(code)) {
							val resp = JSONObject().put("result", "success").put("code", code.trim().uppercase())
							sendResponse(output, 200, "OK", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						} else {
							val resp = JSONObject().put("result", "error").put("error", "Missing or invalid code")
							sendResponse(output, 400, "Bad Request", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						}
					}
					"/psstream/validate-code", "/astraplay/validate-code" -> {
						val code = json.optString("code")
						if (isCodeValid(code)) {
							val resp = JSONObject().put("result", "valid").put("code", code.trim().uppercase())
							sendResponse(output, 200, "OK", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						} else {
							val resp = JSONObject().put("result", "invalid").put("error", "Invalid or expired code")
							sendResponse(output, 404, "Not Found", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						}
					}
					"/psstream/connect", "/astraplay/connect" -> {
						val code = json.optString("code")
						val npsso = json.optString("npsso")
						if (code.isNotEmpty() && npsso.isNotEmpty() && setToken(code, npsso)) {
							val resp = JSONObject().put("result", "success")
							sendResponse(output, 200, "OK", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						} else {
							val resp = JSONObject().put("result", "error").put("error", "Invalid code or expired")
							sendResponse(output, 404, "Not Found", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						}
					}
					"/psstream/get-tokens", "/astraplay/get-tokens" -> {
						val code = json.optString("code")
						val token = getToken(code)
						if (token != null) {
							val resp = JSONObject().put("result", "success").put("npsso", token)
							sendResponse(output, 200, "OK", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						} else if (isCodeValid(code)) {
							val resp = JSONObject().put("result", "pending")
							sendResponse(output, 200, "OK", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						} else {
							val resp = JSONObject().put("result", "error").put("error", "Code not found or expired")
							sendResponse(output, 404, "Not Found", "application/json", resp.toString().toByteArray(Charsets.UTF_8))
						}
					}
					else -> {
						sendResponse(output, 404, "Not Found", "text/plain", "Not Found".toByteArray(Charsets.UTF_8))
					}
				}
				return
			}

			sendResponse(output, 405, "Method Not Allowed", "text/plain", "Method Not Allowed".toByteArray(Charsets.UTF_8))
		} catch (e: Exception) {
			Log.w(TAG, "Error handling client connection", e)
		} finally {
			try { socket.close() } catch (_: Exception) {}
		}
	}

	private fun sendResponse(output: OutputStream, status: Int, statusText: String, contentType: String, data: ByteArray, contentLength: Int = data.size) {
		val header = "HTTP/1.1 $status $statusText\r\n" +
			"Content-Type: $contentType\r\n" +
			"Content-Length: $contentLength\r\n" +
			"Access-Control-Allow-Origin: *\r\n" +
			"Access-Control-Allow-Methods: GET, POST, OPTIONS, HEAD\r\n" +
			"Access-Control-Allow-Headers: Content-Type\r\n" +
			"Connection: close\r\n\r\n"
		output.write(header.toByteArray(Charsets.UTF_8))
		if (data.isNotEmpty()) {
			output.write(data)
		}
		output.flush()
	}

	private fun readLine(input: InputStream): String? {
		val sb = StringBuilder()
		var c = input.read()
		if (c == -1) return null
		while (c != -1 && c != '\n'.code) {
			if (c != '\r'.code) {
				sb.append(c.toChar())
			}
			c = input.read()
		}
		return sb.toString()
	}

	private fun fallbackHtml(): String {
		return """
			<!DOCTYPE html>
			<html><head><title>AstraPlay PSN Login</title></head>
			<body style="font-family:sans-serif;text-align:center;padding:40px;">
			<h2>AstraPlay PlayStation Login</h2>
			<p>Please open the official AstraPlay setup page.</p>
			</body></html>
		""".trimIndent()
	}
}
