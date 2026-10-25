# TsVitch 0.6.2 — Refael Bar

## מה השתנה

- הוסר השימוש בלחיצה על הסטיק הימני. פתיחת EPG מכרטיס ערוץ הועברה ל־**ZL**. חץ למעלה בנגן ממשיך לפתוח את פרטי התוכן והמדריך.
- תוקן מסלול החצים בסרגל הצד: ניתן להגיע ללשונית VOD בחצים ולהיכנס אליה עם A או ימינה, בלי טאץ׳.
- מסכי ה־VOD מוצגים כקוביות: סרטים/סדרות ← קטגוריה ← סדרה ← עונה ← פרק. החצים מנווטים, **A** בוחר, **B** חוזר. כפתור חזרה זמין גם בטאץ׳. הפוקוס נשמר כשחוזרים למסך הקודם.
- **נוספו לאחרונה** ו־**נצפו לאחרונה** מופיעים בנפרד בסרטים ובסדרות. בסדרות, היסטוריית הצפייה מרכזת את הפרקים תחת הסדרה המתאימה.
- סרטים ופרקים מציגים התקדמות וזמן עצירה בתוך הקובייה. A ממשיך אוטומטית מהמיקום השמור. **X** פותח פרטים ואפשרות להתחיל מחדש.
- שמירת מיקום נעשית כל 15 שניות בזמן הניגון, בהשהיה, במעבר תוכן וביציאה. אין פקיעה לאחר 30 יום. נתוני המיקום מהגרסאות הקודמות נקראים לצורך המשך צפייה.
- סיום תוכן נרשם כ״נצפה״ בלי להחזיר מיקום ישן. סרט שנצפה עד הסוף מתחיל מההתחלה בבחירה חוזרת.
- בסיום פרק מופעל הפרק הבא לפי סדר עונות ופרקים, גם בין עונות. התור כולל את כל פרקי הסדרה גם כשנבחר פרק מעונה מסוימת או מתוך תוצאות חיפוש. סרטים אינם עוברים אוטומטית לסרט אחר.
- השינויים חלים על Xtream ו־M3U. תמיכת עברית, אודיו, כתוביות, XMLTV, Catch-up והעדכון מהפורק נשמרו.

## התקנה ועדכון

חלצו את `TsVitch-Switch.zip` לשורש כרטיס הזיכרון או החליפו את ה־NRO הקיים. אפשר גם להתקין את העדכון מתוך היישום. הפעילו במצב יישום מלא. ההגדרות הקיימות נשמרות.

## Technical notes and validation

VOD navigation now uses the same recycling-grid navigation mechanism as the channel browser, with four columns, reusable poster cells, back controls, focus restoration, loading/retry/empty states and guarded asynchronous callbacks. Large catalogues share playback queues instead of copying the entire catalogue into every tile.

Watch state is persisted separately from the original ten-entry channel history. Movies and series have separate recent shelves; recently watched series are deduplicated by provider and series identity. Watched items are matched against the active catalogue, refreshing movie URLs from current provider data. First-discovery dates are persisted separately from playback checkpoints so saving progress does not rewrite the full catalogue index.

Provider `added`/`last_modified` timestamps determine recently added ordering when available. For M3U entries without an added timestamp, ordering uses the date first discovered by this application; the initial import cannot reveal a historical provider addition date. The recent shelves display up to 200 items. Series/episode grouping and ordering depend on the provider or playlist metadata.

Host tests exercise legacy position migration, positions near the start and end, completion/restart behavior, persistence across reopening, write failures, provider isolation, refreshed movie URLs, recently watched series deduplication, recent-addition dates and ordered episode transitions across seasons. Existing playlist, catch-up, cache and Hebrew tests also run. The release pipeline validates the NRO version, developer name and embedded resources.

Controller traversal, rendering, real-provider playback and update/relaunch still require testing on a physical Switch. No Switch device or IPTV subscription credentials were available in this environment. Resume is limited by the seek capability of the supplied stream; elapsed time is shown when total duration is unavailable. Catch-up availability and seek granularity remain provider-dependent.
