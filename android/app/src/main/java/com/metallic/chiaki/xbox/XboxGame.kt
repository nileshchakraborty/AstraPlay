// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.xbox

data class XboxGame(
    val titleId: String,
    val productId: String,
    val name: String,
    val boxArt: String,
    val categories: List<String>,
    val developer: String,
    val publisher: String,
    val description: String
)
