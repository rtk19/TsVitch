# TsVitch 0.4.0 — rtk19 fork

- Hebrew interface and bundled OFL-licensed Noto Sans Hebrew. FriBidi renders Hebrew and mixed Hebrew/Latin/numeric text after line wrapping, preserving logical text for search and saved data.
- A VOD library tab and Y shortcut: movies and series are loaded separately, grouped by provider categories. Series open seasons and episodes. Search within the selected category.
- Live category names resolved through the Xtream categories endpoint. Numeric/string provider IDs and URL-encoded credentials supported. Credential-bearing Xtream request URLs removed from the Xtream request logs.
- Player audio/subtitle track selectors, automatic Hebrew/English subtitle preference, and bundled subtitle font.
- Correct loading indicator termination after seeking, resumed playback and errors.
- Up in the player opens programme information and EPG. Press the right stick on a channel card to inspect EPG before playback. X retains its favorite action.
- Xtream programme schedules with decoded summaries. Optional XMLTV/XMLTV.gz source in the VOD library, matching by EPG ID, channel ID or display name. Explicit timezone offsets, chronological schedule, current programme selection, and a 30-minute guide cache.
- On-demand metadata, episode summaries, resume playback and viewing history. Legacy history/favorites remain readable; history deduplicates by playback URL to avoid movie/live ID collisions.
- In-app updates follow **rtk19/TsVitch** stable releases. Downloads require the exact TsVitch.nro asset, correct size, NRO signature and GitHub SHA-256 digest. Installation occurs after closing RomFS on exit, retains a .bak, and can relaunch immediately through hbloader.
- Fork build no longer requires the original project's registration, advertising or analytics credentials. The supplied build disables those services.
- Versioned channel cache retains EPG IDs and metadata, validates string lengths, and avoids the previous 300 MiB preallocation.

## Install and controls

Extract TsVitch-Switch.zip to the root of the SD card, or replace your existing NRO with TsVitch.nro. Start using title override / full application mode. Existing configuration is preserved.

Choose Hebrew in Settings → App → Language and restart. Configure your Xtream server and credentials, then use the VOD tab or Y to browse movies/series. Use the player settings panel for audio and subtitle tracks. Up opens the programme panel; right-stick click on a channel card opens its schedule. To use a separate XMLTV source, enter its HTTP(S) URL in the VOD tab.

For future updates, use “Check and install update” in the VOD tab or the install action in the release notification. Select Restart after verification. The NRO must be launched from sdmc:/ and its directory must be writable. A .bak of the prior NRO is retained.

## Validation and limits

Host tests cover stable-version ordering, malformed version rejection, URL encoding including Hebrew and reserved characters, XMLTV UTC offsets and invalid dates, old-media JSON compatibility and metadata persistence. FriBidi tests cover Hebrew and mixed text. The Switch NRO is cross-compiled using the pinned devkitPro image in the build workflow.

Playback, rendering, track changes and update/relaunch behavior still require testing on physical Switch hardware and with a real IPTV subscription; no provider credentials were supplied. EPG/VOD data availability depends on the provider. XMLTV is limited to 96 MiB after decompression and programmes from the previous day through the next 14 days. This release provides schedule lists and programme dialogs; it does not add a timeline grid, recording, DRM support or catch-up streaming. Text editing keeps logical cursor offsets; full RTL mirroring of the application layout is not implemented. Some legacy screens retain English text.
