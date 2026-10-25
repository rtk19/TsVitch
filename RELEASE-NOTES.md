# TsVitch 0.5.0 — Refael Bar

## התקנה ושימוש

חלצו את `TsVitch-Switch.zip` לשורש כרטיס הזיכרון, או החליפו את קובץ ה־NRO הקיים. הפעילו במצב יישום מלא. ההגדרות הקיימות נשמרות.

- התמיכה בסרטים, בסדרות, בקטגוריות, בחיפוש, בתקצירים, בשמע ובכתוביות זמינה גם במצב M3U, לפי הנתונים שנכללו ברשימה.
- שם התוכנית המשודרת כעת מוצג בכרטיס הערוץ. תג **הקלטות** מסמן ערוץ שמצהיר על תמיכה ב־Catch-up מוכר.
- **חץ למעלה בנגן**, או **לחיצה על הסטיק הימני בכרטיס ערוץ**, פותחים EPG. בחרו תוכנית זמינה ואז **צפייה בהקלטה**.
- ניתן לבחור הקלטה גם לפי תאריך, שעה ומשך צפייה, ללא תלות בכך שהתוכנית מופיעה ב־EPG.
- בניגון הקלטה, ציר ההתקדמות מייצג את כל משך התוכנית. ניתן להתקדם ולחזור באמצעות הסרגל ופקדי החיפוש של הנגן. אם הזרם אינו תומך בחיפוש מקומי, הנגן מבקש מהספק זרם מהמועד שנבחר.
- **לחיצה על הסטיק השמאלי** מחזירה מיד לשידור החי. L/R עוברים בין ההקלטות הזמינות ברשימת התוכניות שנפתחה. בסיום הקלטה מופעלת הבאה ברשימה, ובסיום הרשימה חוזרים לשידור החי.
- שם המפתח במידע היישום וב־NACP הוא **Refael Bar**. זכויות וקרדיטים של הפרויקט המקורי נשמרים.

## Implementation

- One shared playback model preserves M3U and Xtream VOD, series, EPG and archive metadata through history, favorites and the versioned channel cache.
- M3U parsing supports quoted attributes, commas inside attributes, relative URLs, entries without tvg-id, EXTGRP, automatic XMLTV discovery from x-tvg-url/url-tvg, descriptions, explicit media types, positive VOD durations, series/season/episode attributes and SxxExx title detection.
- M3U VOD is browsable by category, series, season and episode with in-category search. M3U movie/episode metadata is displayed directly without requiring an Xtream account.
- Catch-up supports explicit default templates, append, shift, Flussonic/fs and Xtream/xc endpoints. Supported placeholders: {utc}, {start}, {utcend}, {end}, {duration}, {lutc}, {offset}, {Y}, {m}, {d}, {H}, {M}, {S}, ${start}, ${end}, ${duration}.
- Archive flags, retention windows and catchup-correction are honored. Xtream archive capability/duration and the server's reported clock offset are preserved. Unknown template placeholders are rejected before playback.
- Recorded playback uses a programme-wide timeline, resumable position, native seek where available, server-side seek otherwise, bounded playback duration and a return-to-live action. Pause keeps archive playback available.
- XMLTV guide data is primed in the background and channel cards refresh their current-programme labels every five seconds. The guide retains available history for the preceding 31 days and upcoming programmes for 14 days, with a 30-minute feed cache.
- Hebrew UI/rendering, audio/subtitle selectors and verified updates from rtk19/TsVitch remain available for both source modes.

## Data requirements and validation

A playlist must contain, or identify, the data it offers. Missing VOD entries, series names, descriptions or guide programmes cannot be reconstructed from an arbitrary live-only M3U. Use media-type/type, series-title/tvg-series, season/tvg-season, episode/tvg-episode and description/tvg-description attributes where available. Configure an XMLTV URL manually when the playlist does not advertise one.

Catch-up requires a supported provider endpoint or catchup-source template. Availability depends on the provider's retention and subscription. Xtream server seeking may be limited to minute boundaries; the timeline reflects that position, while native seeking can be finer where supported. XMLTV alone does not enable recordings. The date/time picker uses the application's local-time clock, exposes advertised retention up to 366 days, and offers the most recent seven days when retention is unspecified. XMLTV/XMLTV.gz is limited to 96 MiB after decompression. This does not add local DVR recording or DRM support.

Host tests cover M3U metadata and relative URLs, legacy saved-data compatibility, channel-cache metadata and corruption, catch-up URL construction, retention/seek boundaries, time corrections, version ordering, Hebrew encoding and bidirectional rendering. The Switch release pipeline validates NRO assets, embedded version, developer name and Hebrew resources before publishing.

Playback, rendering, remote archive interoperability and in-app update/relaunch still require physical Switch testing with a real provider. No IPTV subscription credentials were supplied for this session.
