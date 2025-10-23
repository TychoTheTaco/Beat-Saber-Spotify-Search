#pragma once

#include <atomic>
#include <queue>

#include "HMUI/ImageView.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/HorizontalLayoutGroup.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "custom-types/shared/macros.hpp"
#include "song-details/shared/SongDetails.hpp"

#include "SpotifyClient.hpp"
#include "UI/TableViewDataSources/DownloadHistoryTableViewDataSource.hpp"
#include "CustomSongFilter.hpp"

#if HOT_RELOAD
#include "bsml/shared/BSML/ViewControllers/HotReloadViewController.hpp"
using BaseViewController = BSML::HotReloadViewController;
#else
#include "HMUI/ViewController.hpp"
using BaseViewController = HMUI::ViewController;
#endif

namespace SpotifySearch::Filter {

using SongFilterFunction = std::function<bool(const SongDetailsCache::Song* const song, const spotify::Track& track)>;
using SongScoreFunction = std::function<int(const spotify::Track& track, const SongDetailsCache::Song& song)>;

extern SongFilterFunction DEFAULT_SONG_FILTER_FUNCTION;
extern SongScoreFunction DEFAULT_SONG_SCORE_FUNCTION;

}

DECLARE_CLASS_CODEGEN_INTERFACES(SpotifySearch::UI::ViewControllers, MainViewController, BaseViewController) {

    DECLARE_CTOR(ctor);

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool isFirstActivation, bool addedToHierarchy, bool screenSystemDisabling);

    DECLARE_INSTANCE_METHOD(void, PostParse);

    // Spotify songs and playlists lists

    // Header
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, spotifyColumnTitleTextView_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, playlistsMenuButton_);
    DECLARE_INSTANCE_METHOD(void, onPlaylistsMenuButtonClicked);

    // Playlist list
    DECLARE_INSTANCE_FIELD(UnityW<BSML::CustomListTableData>, spotifyPlaylistListView_);
    DECLARE_INSTANCE_METHOD(void, onPlaylistSelected, UnityW<HMUI::TableView> table, int id);

    // Track list
    DECLARE_INSTANCE_FIELD(UnityW<BSML::CustomListTableData>, spotifyTrackListView_);
    DECLARE_INSTANCE_METHOD(void, onTrackSelected, UnityW<HMUI::TableView> table, int id);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, randomTrackButton_);
    DECLARE_INSTANCE_METHOD(void, onRandomTrackButtonClicked);

    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::HorizontalLayoutGroup>, spotifyTrackListLoadingIndicatorContainer_);

    // Error message container
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::VerticalLayoutGroup>, spotifyListViewErrorContainer_);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, spotifyTrackListErrorMessageTextView_);
    DECLARE_INSTANCE_METHOD(void, onSpotifyTrackListRetryButtonClicked);

    // Status message container
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::VerticalLayoutGroup>, spotifyListViewStatusContainer_);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, spotifyTrackListStatusTextView_);

    // Search results
    DECLARE_INSTANCE_FIELD(UnityW<BSML::CustomListTableData>, searchResultsList_);
    DECLARE_INSTANCE_METHOD(void, onSearchResultSelected, UnityW<HMUI::TableView> table, int id);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::HorizontalLayoutGroup>, searchResultsListLoadingIndicatorContainer_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::VerticalLayoutGroup>, searchResultsListViewErrorContainer_);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, searchResultsListStatusTextView_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, showAllByArtistButton_);
    DECLARE_INSTANCE_METHOD(void, onShowAllByArtistButtonClicked);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, hideDownloadedMapsButton_);
    DECLARE_INSTANCE_METHOD(void, onHideDownloadedMapsButtonClicked);

    // Custom Song Preview
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, previewSongNameTextView_);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, previewSongAuthorTextView_);
    DECLARE_INSTANCE_FIELD(HMUI::ImageView*, previewSongImage_);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, previewSongUploaderTextView_);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, previewSongLengthTextView_);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, previewSongNPSTextView_);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, previewSongNJSTextView_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, downloadButton_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, playButton_);
    DECLARE_INSTANCE_METHOD(void, onDownloadButtonClicked);
    DECLARE_INSTANCE_METHOD(void, onPlayButtonClicked);

    public:
    void setFilter(const CustomSongFilter& customSongFilter);

    private:
    std::vector<const SongDetailsCache::Song*> searchResultItems_;
    const SongDetailsCache::Song* previewSong_;

    std::unique_ptr<spotify::Playlist> selectedPlaylist_;
    std::unique_ptr<const spotify::Track> selectedTrack_;

    std::queue<std::shared_ptr<DownloadHistoryItem>> pendingDownloads_;
    std::mutex pendingDownloadsMutex_;
    std::atomic<bool> isDownloadThreadRunning_;

    std::atomic<bool> isLoadingMoreSpotifyTracks_;
    std::atomic<bool> isLoadingMoreSpotifyPlaylists_;
    std::atomic<bool> isSearchInProgress_;

    std::atomic<bool> allTracksLoaded_;
    std::atomic<bool> allPlaylistsLoaded_;

    std::atomic<bool> isShowingAllTracksByArtist_;
    std::atomic<bool> isShowingDownloadedMaps_;

    SpotifySearch::Filter::SongFilterFunction currentSongFilter_;
    SpotifySearch::Filter::SongScoreFunction currentSongScore_;

    void reloadSpotifyTrackListView();
    void reloadSpotifyPlaylistListView();

    void setSelectedSongUi(const SongDetailsCache::Song* const song);

    void showSpotifyTrackLoadingIndicator();
    void showSpotifyTrackListView();
    void showSpotifyPlaylistListView();
    void onSpotifyTrackLoadingError(const std::string& message);

    void doSongSearch(const spotify::Track& track);

    std::unique_ptr<CustomSongFilter> customSongFilter_;

    void startDownloadThread();

    void onTrackLoadError(const std::string& message);
    void resetListError();
};
