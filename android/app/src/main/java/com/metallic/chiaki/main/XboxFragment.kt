// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.main

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.GridLayoutManager
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import com.metallic.chiaki.common.Preferences
import com.metallic.chiaki.common.ext.alertDialogBuilder
import com.metallic.chiaki.common.ext.isTv
import com.metallic.chiaki.xbox.XboxAuthManager
import com.metallic.chiaki.xbox.XboxGame
import com.metallic.chiaki.xbox.XboxGameAdapter
import io.github.nileshchakraborty.astraplay.R
import io.github.nileshchakraborty.astraplay.databinding.FragmentXboxBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import java.io.InputStreamReader

class XboxFragment : Fragment() {

    private var _binding: FragmentXboxBinding? = null
    private val binding get() = _binding!!

    private lateinit var preferences: Preferences
    private var allGames: List<XboxGame> = emptyList()
    private var filteredGames: List<XboxGame> = emptyList()
    private lateinit var adapter: XboxGameAdapter

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentXboxBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val context = requireContext()
        preferences = Preferences(context)

        setupRecyclerView()
        setupSearch()
        setupAuthUI()
        loadCatalog()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        XboxAuthManager.cancelLogin()
        _binding = null
    }

    private fun setupRecyclerView() {
        val spanCount = if (resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE) {
            if (requireContext().isTv()) 6 else 4
        } else {
            3
        }

        adapter = XboxGameAdapter(emptyList()) { game ->
            onGameSelected(game.titleId, game.name)
        }

        binding.xboxGamesRecyclerView.layoutManager = GridLayoutManager(requireContext(), spanCount)
        binding.xboxGamesRecyclerView.adapter = adapter
    }

    private fun setupSearch() {
        binding.searchGamesInput.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                filterGames(s?.toString().orEmpty())
            }
            override fun afterTextChanged(s: Editable?) {}
        })

        binding.directStreamButton.setOnClickListener {
            val titleId = binding.titleIdInput.text.toString().trim()
            if (titleId.isEmpty()) {
                Toast.makeText(requireContext(), "Please enter an Xbox Title ID", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            onGameSelected(titleId, titleId)
        }
    }

    private fun filterGames(query: String) {
        val q = query.trim().lowercase()
        filteredGames = if (q.isEmpty()) {
            allGames
        } else {
            allGames.filter {
                it.name.lowercase().contains(q) ||
                it.titleId.lowercase().contains(q) ||
                it.categories.any { cat -> cat.lowercase().contains(q) } ||
                it.publisher.lowercase().contains(q)
            }
        }
        adapter.updateData(filteredGames)
        binding.gameCountText.text = "(${filteredGames.size})"
    }

    private fun setupAuthUI() {
        updateAuthState()

        binding.authActionButton.setOnClickListener {
            val gamertag = preferences.getXboxGamertag()
            if (gamertag != null) {
                // Sign out
                preferences.setXboxGamertag(null)
                preferences.setXboxRefreshToken(null)
                preferences.setXboxUserHash(null)
                updateAuthState()
                Toast.makeText(requireContext(), "Signed out from Xbox", Toast.LENGTH_SHORT).show()
            } else {
                // Start device code login
                startLoginFlow()
            }
        }

        binding.refreshButton.setOnClickListener {
            loadCatalog()
            Toast.makeText(requireContext(), "Refreshed Xbox Cloud Catalog", Toast.LENGTH_SHORT).show()
        }

        binding.copyCodeButton.setOnClickListener {
            val code = binding.userCodeText.text.toString()
            val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("Xbox Code", code)
            clipboard.setPrimaryClip(clip)
            Toast.makeText(requireContext(), "Code copied to clipboard: $code", Toast.LENGTH_SHORT).show()
        }

        binding.cancelLoginButton.setOnClickListener {
            XboxAuthManager.cancelLogin()
            binding.loginContainer.visibility = View.GONE
            binding.contentContainer.visibility = View.VISIBLE
        }
    }

    private fun updateAuthState() {
        val gamertag = preferences.getXboxGamertag()
        if (gamertag != null) {
            binding.gamertagText.text = gamertag
            binding.statusDot.backgroundTintList = android.content.res.ColorStateList.valueOf(Color.parseColor("#4CAF50"))
            binding.authActionButton.text = "Sign Out"
            binding.authActionButton.strokeColor = android.content.res.ColorStateList.valueOf(Color.parseColor("#80FFFFFF"))
        } else {
            binding.gamertagText.text = "Not Logged In"
            binding.statusDot.backgroundTintList = android.content.res.ColorStateList.valueOf(Color.parseColor("#888888"))
            binding.authActionButton.text = "Sign In"
            binding.authActionButton.strokeColor = android.content.res.ColorStateList.valueOf(Color.parseColor("#107C10"))
        }
    }

    private fun startLoginFlow() {
        binding.loginContainer.visibility = View.VISIBLE
        binding.contentContainer.visibility = View.GONE
        binding.pollStatusText.text = "Requesting device code from Microsoft..."
        binding.userCodeText.text = "--------"

        viewLifecycleOwner.lifecycleScope.launch {
            XboxAuthManager.startDeviceCodeLogin(
                context = requireContext(),
                onDeviceCode = { userCode, verificationUri, expiresIn ->
                    if (_binding == null) return@startDeviceCodeLogin
                    binding.userCodeText.text = userCode
                    binding.pollStatusText.text = "Waiting for authentication approval at $verificationUri..."

                    // Generate QR code
                    try {
                        val qrBitmap = generateQrCode(verificationUri, 360)
                        binding.qrCodeImageView.setImageBitmap(qrBitmap)
                    } catch (e: Exception) {
                        Log.e("XboxFragment", "Failed to generate QR code", e)
                    }
                },
                onSuccess = { gamertag ->
                    if (_binding == null) return@startDeviceCodeLogin
                    binding.loginContainer.visibility = View.GONE
                    binding.contentContainer.visibility = View.VISIBLE
                    updateAuthState()
                    Toast.makeText(requireContext(), "Welcome back, $gamertag!", Toast.LENGTH_LONG).show()
                },
                onError = { error ->
                    if (_binding == null) return@startDeviceCodeLogin
                    binding.loginContainer.visibility = View.GONE
                    binding.contentContainer.visibility = View.VISIBLE
                    Toast.makeText(requireContext(), error, Toast.LENGTH_LONG).show()
                }
            )
        }
    }

    private fun generateQrCode(content: String, size: Int): Bitmap {
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

    private fun loadCatalog() {
        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            try {
                val inputStream = requireContext().assets.open("xcloud_catalog.json")
                val jsonStr = InputStreamReader(inputStream).use { it.readText() }
                val jsonArray = JSONArray(jsonStr)

                val list = ArrayList<XboxGame>(jsonArray.length())
                for (i in 0 until jsonArray.length()) {
                    val obj = jsonArray.getJSONObject(i)
                    val catArray = obj.optJSONArray("categories")
                    val categories = ArrayList<String>()
                    if (catArray != null) {
                        for (c in 0 until catArray.length()) {
                            categories.add(catArray.getString(c))
                        }
                    }

                    list.add(
                        XboxGame(
                            titleId = obj.optString("titleId"),
                            productId = obj.optString("productId"),
                            name = obj.optString("name"),
                            boxArt = obj.optString("boxArt"),
                            categories = categories,
                            developer = obj.optString("developer"),
                            publisher = obj.optString("publisher"),
                            description = obj.optString("description")
                        )
                    )
                }

                withContext(Dispatchers.Main) {
                    if (_binding == null) return@withContext
                    allGames = list
                    filterGames(binding.searchGamesInput.text.toString())
                }
            } catch (e: Exception) {
                Log.e("XboxFragment", "Failed to load xcloud catalog", e)
            }
        }
    }

    private fun onGameSelected(titleId: String, gameName: String) {
        val gamertag = preferences.getXboxGamertag()
        if (gamertag == null) {
            requireContext().alertDialogBuilder()
                .setTitle("Xbox Sign In Required")
                .setMessage("Please sign in to your Xbox account before starting a cloud stream session.")
                .setPositiveButton("Sign In") { _, _ -> startLoginFlow() }
                .setNegativeButton("Cancel", null)
                .show()
            return
        }

        // Inform user about subscription requirement gracefully without crashing
        requireContext().alertDialogBuilder()
            .setTitle(gameName)
            .setMessage("Starting stream for $gameName ($titleId).\n\nXbox Cloud Gaming requires an active Xbox Game Pass Ultimate subscription.")
            .setPositiveButton("Launch") { _, _ ->
                Toast.makeText(
                    requireContext(),
                    "Xbox Cloud Gaming requires an active Xbox Game Pass Ultimate subscription.",
                    Toast.LENGTH_LONG
                ).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }
}
