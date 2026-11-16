// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

package com.metallic.chiaki.xbox

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import coil.load
import io.github.nileshchakraborty.astraplay.R

class XboxGameAdapter(
    private var games: List<XboxGame>,
    private val onItemClick: (XboxGame) -> Unit
) : RecyclerView.Adapter<XboxGameAdapter.ViewHolder>() {

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val gameImageView: ImageView = view.findViewById(R.id.gameImageView)
        val gameNameTextView: TextView = view.findViewById(R.id.gameNameTextView)
        val gameCategoryTextView: TextView = view.findViewById(R.id.gameCategoryTextView)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_xbox_game, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val game = games[position]
        holder.gameNameTextView.text = game.name
        holder.gameCategoryTextView.text = game.categories.firstOrNull() ?: game.publisher
        holder.gameImageView.load(game.boxArt) {
            crossfade(true)
        }
        holder.itemView.setOnClickListener {
            onItemClick(game)
        }
    }

    override fun getItemCount(): Int = games.size

    fun updateData(newGames: List<XboxGame>) {
        games = newGames
        notifyDataSetChanged()
    }
}
