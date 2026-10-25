## 0.6.5

Cache VOD catalogues, categories and series details in memory and on disk, coalesce requests, reuse the live M3U download, refresh stale data in the background and support explicit refresh. Replace repeated catalogue scans and per-tile queue copies with shared snapshots and indices. Retain unchanged grid pages and release their owned data sources on close.

Add provider-scoped pinned live categories, separate movie/whole-series favorites shelves with Y actions and badges, and a subtle cached glyph embolden for all text languages. Expand real-UI tests to 4,000 movies/1,000 series, count requests on repeated opens and process restart, and verify pin/favorite actions and manual refresh followed by Back.

## 0.6.4

Fix startup failures while creating channel cards: move catch-up badge padding to its Box container and defer recycler cell rebuilding outside Yoga layout callbacks. Lazily construct inactive tabs. Add a real desktop UI regression run with local M3U/Xtream fixtures and Hebrew, including VOD focus and Settings.

Unify Check for updates in Settings with the NX-torrent-player flow: release/changelog prompt, streamed download with percent/size/bar, on-disk SHA-256/NRO verification, persistent staging, backup and rollback, and loader relaunch with HOME exit disabled. Use a plain ZIP without extra file attributes and validate it before publishing.

## 0.6.3

Keep one canonical watch record when the same media is opened from legacy favorites/history and the VOD catalogue. Preserve richer series metadata when updating or resetting progress.

## 0.6.2

Restore VOD focus only after the outgoing activity has finished fading, without stealing focus from a player or dialog.

## 0.6.1

Restore the selected VOD tile after the framework restores its activity focus, including recently watched pages whose ordering changes.

## 0.6.0

Controller-accessible VOD tile navigation, no right-stick-click actions, persistent watch progress and completion, recently added/watched movie and series shelves, and automatic episode queues across seasons.

## 0.5.1

Preserve explicit live-channel metadata when a category name contains Movies or Series. Includes all M3U/catch-up changes from 0.5.0.

## 0.5.0

Shared M3U/Xtream catalogues and archive playback, programme timelines with seek and return to live, current-programme channel cards and archive badges. Developer: Refael Bar. See [release notes](RELEASE-NOTES.md).

## 0.4.0 (rtk19 fork)

Hebrew rendering, categorized Xtream VOD/live TV, player track selection, programme details and XMLTV EPG, and verified in-app updates from this fork. See [release notes](RELEASE-NOTES.md) for installation, validation and limitations.

# Changelog

All notable changes to this project will be documented in this file.

---

## [0.3.2] - Hot Fix

### 🔧 Build & Compatibility

- **Recompiled with the latest libnx** to ensure compatibility with the current devkitPro toolchain and Switch SDK.
- No functional changes; this hotfix focuses on build stability and forward compatibility.

---

## [0.3.1] - Bug Fixes

### 🐛 Bug Fixes

#### macOS Compatibility
- **Fixed macOS build compatibility** with proper i18n string handling in live stream error dialog
- **Fixed unsafe type casting** in platform detection - replaced unchecked cast with safe null checks in settings, contributors, and sponsors views
- **Fixed missing header include** - added `<thread>` header in ChannelManager.cpp for proper compilation
- **Fixed DownloadManager constructor initialization order** on macOS for member variable initialization compliance

#### Crash Fixes
- **Fixed segmentation fault in HomeLive destructor** - initialized `exitEventSubscription` member variable to prevent undefined behavior on app exit
- **Fixed memory safety issue** in event subscription cleanup

#### Initialization & Type Safety
- **Fixed platform pointer null checks** to prevent crashes when platform is unavailable
- **Fixed string localization** in live download error dialogs to use proper brls::getStr() instead of macro expansion

### 📊 Affected Components
- ChannelManager
- DownloadManager  
- SettingsActivity
- ContributorsView
- SponsorsView
- StreamHelper utilities
- HomeLive fragment

### 🔧 Technical Details
- Improved type safety by removing unsafe casts
- Enhanced macOS compatibility layer
- Fixed member initialization order in structs
- Proper initialization of event subscription members to prevent crashes on destruction

---

## [0.3.0] - Happy Holidays

### 🎉 Major Features

#### 📥 Full Download System
- **Download Manager** with multi-threaded + chunked downloads (optimized for Nintendo Switch)
- **Downloads Tab** to monitor active and completed downloads
- **Advanced Controls**: pause, resume, cancel
- **Auto Cover Fetch** for each downloaded video
- **Live Progress** with speed and ETA
- **Persistent State**: resume incomplete downloads after restart
- **Parallel Downloads** to maximize throughput

#### 📺 Xtream Codes IPTV
- **Full Xtream API** integration for IPTV providers
- **IPTV Mode Switcher** between M3U8 and Xtream
- **Category-Based** channel organization
- **Secure Auth** for Xtream servers
- **⚡ 4.5x Faster Loading** via optimized caching

#### ⏯️ Playback Position Restore
- **Smart Auto-Resume**: videos continue where you left off
- **Mode-Aware**: differentiates live streams vs VOD
- **Persistent Cache** with 30-day expiry
- **Smart Tracking**: ignores positions too close to start or end

#### 🚫 Live Download Prevention
- **Auto Detection** of live content by URL/title heuristics
- **Live Download Block** with clear, localized errors
- **Centralized Helper** for consistent behavior app-wide

#### 👥 Contributors & Sponsors
- **Contributors View** with avatars and stats from GitHub
- **Sponsors View** to highlight supporters
- **QR Codes** for quick access to repo and sponsor page
- **GitHub API** (REST + GraphQL) with 24h cache

#### 📺 Ad Handling Improvements
- **Automatic Fallback**: if an ad fails, skip to content
- **No Interruptions** even when ad servers are down

### 🎨 UI/UX Improvements

- **Video Progress Slider** more responsive with better gestures
- **Improved OSD** for clearer on-screen controls
- **Settings Reorg** with a refreshed About section
- **Fullscreen** control handling polished

### 🔧 Technical Improvements

#### Performance
- **350% faster** channel loading
- **Memory Optimized** RecyclingGrid
- **Better Threading** for background tasks
- **Crash Fixes** on app shutdown

#### Networking & API
- **HTTP Chunked Transfer** for large downloads
- **Smarter Timeouts** for slow networks
- **Batch Requests** to cut latency
- **Robust Error Handling** on network calls

#### Channel System
- **Full Refactor** with multi-source support
- **Event System** for IPTV change notifications
- **Download Detection** of already-downloaded items
- **Optimized Cache** for metadata and playlists

### 🌍 Localization

New strings in **Italian**, **English**, and **Brazilian Portuguese**:
- `live_download_error` / `live_download_error_desc`
- `contributors_header` / `sponsors_header`
- `contrib_text` / `sponsor_qr`

### 🐛 Bug Fixes

**Stability**
- Fixed crashes on activity shutdown
- Fixed cache memory leak
- Fixed download race conditions
- Fixed thread sync issues

**Functionality**
- Fixed IPTV channel cache
- Fixed Switch download banner
- Fixed duplicate download IDs
- Fixed live vs on-demand detection
- Fixed runtime IPTV mode switching

**UI/UX**
- Fixed RecyclingGrid layout
- Fixed controller focus navigation
- Fixed transition glitches
- Fixed progress bars across themes

### 📦 Dependencies & Build

- **Borealis UI** updated (submodule)
- **OpenCC** updated (submodule)
- **Crowdin** configured for translations
- **GitHub Actions** for automatic download badges
- **3 New SVG Assets** for download UI

### 📊 Release Stats

- **76 files changed** (+8,621 / -322 lines)
- **19 new files** added
- **5 new classes**: DownloadManager, DownloadProgressManager, XtreamAPI, PlaybackPositionManager, Contributors/SponsorsView
- **4 refactored classes**: ChannelManager, VideoView, LivePlayerActivity, HomeFragments

### 💡 Installation Notes

**Requirements**:
- Nintendo Switch with Atmosphère/CFW
- Enough storage for downloads (2GB+ recommended)
- Internet connection for streaming and downloads

**Compatibility**:
- ✅ Nintendo Switch
- ✅ M3U8 playlists
- ✅ Xtream Codes servers
- ⚠️ Xtream credentials required for compatible providers

**Install**:
1. Download `TsVitch.nro` from the release
2. Copy to `/switch/` on the SD card
3. Launch from Homebrew Launcher
4. Configure playlist/Xtream in settings


### Disclaimers
- **Legal Compliance**: The app does not host or provide IPTV content. Users must ensure their playlists comply with local laws.
- **Educational Purpose**: Provided "as-is" without warranties. The author disclaims responsibility for misuse or damages.

---

This release lays the foundation for turning your Nintendo Switch into a multimedia powerhouse! 🚀