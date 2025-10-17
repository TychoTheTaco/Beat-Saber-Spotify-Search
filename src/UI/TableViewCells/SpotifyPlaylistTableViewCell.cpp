#include "UnityEngine/GameObject.hpp"

#include "Log.hpp"
#include "UI/TableViewCells/SpotifyPlaylistTableViewCell.hpp"
#include "Utils.hpp"
#include "SpriteCache.hpp"

DEFINE_TYPE(SpotifySearch::UI, SpotifyPlaylistTableViewCell);

using namespace SpotifySearch::UI;

void SpotifyPlaylistTableViewCell::ctor() {
    INVOKE_BASE_CTOR(classof(HMUI::TableCell*));
    playlist_ = nullptr;
}

void SpotifyPlaylistTableViewCell::OnDestroy() {
}

void SpotifyPlaylistTableViewCell::SelectionDidChange(HMUI::SelectableCell::TransitionType transitionType) {
    updateBackground();
}

void SpotifyPlaylistTableViewCell::HighlightDidChange(HMUI::SelectableCell::TransitionType transitionType) {
    updateBackground();
}

void SpotifyPlaylistTableViewCell::WasPreparedForReuse() {
}

void SpotifyPlaylistTableViewCell::updateBackground() {
    root_->set_color(UnityEngine::Color(0, 0, 0, selected || highlighted ? 0.8f : 0.45f));
}

void SpotifyPlaylistTableViewCell::setPlaylist(const spotify::Playlist& playlist) {
    playlist_ = std::make_unique<spotify::Playlist>(playlist);

    // Name
    playlistNameTextView_->set_text(playlist_->name);

    // Track count
    songCountTextView_->set_text(std::format("{} tracks", playlist_->totalItemCount));

    // Loading sprite
    const UnityW<UnityEngine::Sprite> placeholderSprite = Utils::getPlaylistPlaceholderSprite();
    image_->set_sprite(placeholderSprite);

    // Load cover image
    if (!playlist_->imageUrl.empty()) {
        const std::string playlistId = playlist_->id;
        Utils::getImageAsSprite(playlist_->imageUrl, [this, playlistId](const UnityW<UnityEngine::Sprite> sprite) {
            // Check if the selected song has changed
            if (!playlist_ || playlist_->id != playlistId) {
                SpotifySearch::Log.warn("Cancelled sprite update");
                return;
            }

            // Update UI
            if (sprite) {
                image_->set_sprite(sprite);
            } else {
                SpotifySearch::Log.warn("Failed loading cover image for playlist with hash: {}", playlistId);
            }
        });
    }

}
