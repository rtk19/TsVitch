# TsVitch 0.6.4 — Refael Bar

## תיקון קריסה ופתיחה

- תוקנה קריסה ביצירת כרטיס הערוץ הראשון: תג ההקלטות השתמש במאפיין `padding` שאינו נתמך על Label. הריווח נמצא כעת במכל Box מתאים.
- בניית תאי הגריד הועברה מחוץ ל־Yoga layout callbacks כדי למנוע חישובי פריסה חוזרים וקריסה בזמן טעינה או שינוי רוחב.
- לשוניות שאינן מוצגות נטענות רק כשנכנסים אליהן, כדי לצמצם את העבודה והזיכרון בזמן הפתיחה.
- הניווט בקוביות, המשך הצפייה, ההתקדמות, המדפים האחרונים והפעלת הפרק הבא נשמרו.

## עדכון מתוך האפליקציה

העדכון נמצא כעת ב־**Settings → Tools → Check for updates**, ואינו נמצא בתפריט VOD.

ההתנהגות תואמת למנגנון של [NX-torrent-player](https://github.com/rtk19/NX-torrent-player/blob/main/app/update.cpp):

1. בדיקת גרסה מהפורק `rtk19/TsVitch` והצגת גרסה זמינה, גודל ופירוט שינויים.
2. בחירה ב־Update, הורדה עם פס התקדמות, אחוזים וגודל שהורד. ההורדה נכתבת ישירות לכרטיס.
3. בדיקת גודל, מבנה NRO ו־SHA-256, כולל קריאה חוזרת מהכרטיס, לפני הכנת ההתקנה.
4. Restart מסיים את היישום, משחרר RomFS, מחליף את ה־NRO ושומר את הקודם כ־`.bak`.
5. היישום מפעיל את מסלול הטעינה מחדש של hbloader עם אותו נתיב NRO. יציאה למסך HOME מבוטלת למסלול זה. במפעיל שאינו תומך בטעינה מחדש תופיע הודעה לסגור ולפתוח ידנית.

הורדה חלקית או שגויה אינה מחליפה את האפליקציה. עדכון שהוכן ואומת נשמר בין הפעלות; כשל בהחלפה מנסה לשחזר את הגיבוי ומתועד להפעלה הבאה.

## התקנה כשגרסה קודמת קורסת

הורידו **TsVitch.nro** ישירות והחליפו את ה־NRO הקיים בכרטיס. ההגדרות ונתוני הצפייה נשמרים. לחלופין חלצו **TsVitch-Switch.zip** לשורש הכרטיס. ה־ZIP נארז בפורמט רגיל בלי מאפייני קבצים נוספים ונבדק לפני הפרסום. הפעילו במצב יישום מלא דרך hbmenu.

## Validation and limits

The regression suite runs the real desktop application with an isolated profile and a local HTTP provider, imports 500 live channels in M3U and Hebrew Xtream modes, traverses VOD focus/nested grids and opens Settings. This exercises actual Borealis XML inflation, Yoga layout and rendering, rather than checking embedded resource presence alone. Release publication is gated on this runtime job, host tests and the Switch cross-build.

Host tests cover NRO/ASET bounds, staged file replacement and preservation of the installed file on failures, in addition to existing playlist, catch-up, Hebrew, cache and watch-progress tests. Release files include SHA256SUMS and a checked ZIP.

No physical Switch or iPhone was available for testing. Actual hbloader relaunch and SD installation still require device verification. Real-provider playback, EPG and catch-up availability depend on provider metadata and stream seek support.
