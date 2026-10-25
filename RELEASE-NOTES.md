# TsVitch 0.6.5 — Refael Bar

## VOD מהיר יותר

- קטלוגים ופרטי סדרות נשמרים בזיכרון ובכרטיס, כולל אחרי הפעלה מחדש. מעבר לקטגוריה ופתיחה חוזרת של סדרה משתמשים בנתונים שכבר נטענו.
- בקשות מקבילות לאותם נתונים מאוחדות. רשימת M3U שכבר ירדה עבור הערוצים משמשת גם את ה־VOD.
- במקורות Xtream, רשימת התוכן ושמות הקטגוריות נטענים במקביל.
- הנתונים משותפים בין הקוביות; חיפושים חוזרים בכל הקטלוג הוחלפו באינדקסים. מסך שלא השתנה אינו נבנה מחדש בכל חזרה אליו.
- נתונים בני יותר משש שעות מוצגים מיד בזמן רענון ברקע. **ZL** או כפתור **רענון ספרייה** מרעננים ידנית. כשל ברענון משאיר את הספרייה השמורה זמינה.
- מטמון הזיכרון מוגבל ל־14 תמונות מצב ומטמון הכרטיס ל־64 קבצים. נתוני מסכי VOD נסגרים משתחררים כדי למנוע הצטברות זיכרון בניווט חוזר.

הטעינה הראשונה עדיין תלויה בגודל הקטלוג ובשרת הספק. פריט שכבר פונה מהמטמון ייטען שוב לפי הצורך.

## קטגוריות ומועדפים

- **Y על קטגוריית ערוצים** מצמיד אותה לראש הרשימה או מבטל הצמדה. **X** פותח אפשרויות לקטגוריה. קטגוריות מוצמדות מסומנות ונשמרות בין הפעלות; הקטגוריה שנבחרה נשמרת לפי שמה גם כשהסדר משתנה.
- **מועדפים** מופיעים בנפרד בסרטים ובסדרות. **Y על קובייה** מוסיף או מסיר, וכוכב מסמן פריט שמור.
- בסדרות נשמרת **הסדרה כולה**. אפשר לסמן גם מתוך מסך העונות או דרך פרטים / אפשרויות. X בסרט או בפרק פותח פרטים, המשך צפייה ואפשרויות הכוללות מועדפים והתחלה מחדש.
- המועדפים וההצמדות מופרדים לפי מקור IPTV. נתוני Xtream מזוהים לפי ספק וחשבון כך שהחלפת סיסמה אינה מוחקת את המועדפים.
- השינויים חלים על Xtream ו־M3U בהתאם למטא־נתונים שהספק או הרשימה מספקים.

## קריאות טקסט

כל גופני הטקסט מקבלים עיבוי עדין בזמן יצירת האות במטמון הגופן, כולל עברית, לטינית, ערבית ושפות מזרח אסיה. רוחב התווים, הריווח ושבירת השורות נשמרים. סמלי מקשים ואמוג׳י אינם מעובים.

## התקנה ועדכון

**Settings → Tools → Check for updates** → Update → Restart.

להתקנה ידנית החליפו את `TsVitch.nro` הקיים או חלצו `TsVitch-Switch.zip` לשורש הכרטיס. ההגדרות ונתוני הצפייה נשמרים. מומלץ להפעיל במצב יישום מלא.

## Validation and limits

The real desktop UI fixture imports 500 live channels, 4,000 movies and 1,000 series in M3U and Hebrew Xtream modes. It invokes pin/favorite actions, navigates categories, seasons and episodes, revisits the same catalogue/series, performs an explicit refresh followed by Back, then restarts the process and checks persisted preferences. Provider request counters ensure repeated opens and process restart add no catalogue/episode requests beyond the initial load and explicit refresh.

Host tests cover provider isolation, movie and whole-series favorite persistence, pin/unpin ordering, selected-category preservation, failed writes, cache expiry and reopening, and fractional glyph weight across languages. Existing playback progress, playlist, catch-up, update verification and Hebrew tests remain enabled. Release publication requires the host tests, real UI tests and Switch cross-build.

No physical Switch was available. Actual console performance, rendering and playback still require device verification. Series grouping depends on playlist/provider metadata; cached data reflects the last successful refresh.
