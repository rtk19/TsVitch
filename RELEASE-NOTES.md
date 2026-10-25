# TsVitch 0.6.6 — Refael Bar

## תצוגה וקריאות

- כפתורי חזרה, חיפוש ורענון כוללים אייקוני SVG וכיתוב גדול. החיפוש ב־VOD עבר לסרגל העליון ונגיש גם באמצעות **L**.
- כרטיסי ערוצים מציגים לוגו לצד שם גדול, שם התוכנית הנוכחית, שעות ופס התקדמות, וסימון ארכיון ומועדפים לפי זמינות.
- כרטיסי סרטים וסדרות מציגים פוסטרים; כרטיסי קטגוריות ופרקים מותאמים בנפרד. כשהספק אינו מספק תמונה תקינה מוצג סמל ברור.
- תיקון שדות תמונה חלופיים, כתובות יחסיות, מערכי תמונות ו־WebP עם פרמטרים בכתובת; נוספה טעינת SVG. התמונות מוקטנות לפני העלאה ל־GPU, עם מגבלות הורדה ופענוח וביטול בעת גלילה.
- פונט Noto שהגיע במשקל ברירת מחדל 100 הוחלף בגרסת Medium קבועה (500), המתאימה למנוע הגופנים של Switch. העיבוי העדין לשאר גופני הטקסט נשמר.
- רקע הבחירה אינו מסתיר טקסט לבן. שינויי רוחב זעירים בפריסת הקוביות אינם גורמים עוד לבנייה חוזרת וביטול טעינות תמונה.

## חיפוש בעברית

- מקלדת בתוך האפליקציה לחיפוש בערוצים, בסרטים, בסדרות ובפרקים, עם חצים וטאץ׳. אין צורך בתמיכה בעברית במקלדת המערכת.
- מעבר בין עברית, אנגלית ומספרים, רווח, מחיקה, ניקוי והזזת סמן. אפשר לפתוח גם את מקלדת המערכת עבור שפות נוספות וקלט רגיל.
- **A** מקליד, **X** מוחק, **Y** מחליף פריסה, **L/R** מזיזים את הסמן, **+** מבצע חיפוש. הפריסה הראשונית מתאימה לשפת הממשק והאחרונה נשמרת במהלך ההפעלה.
- חיפוש בעברית מתעלם מניקוד ומההבדל בין אותיות סופיות לרגילות; חיפוש לועזי אינו תלוי ברישיות. עריכת UTF-8 ומיקום הסמן בטקסט מעורב טופלו בנפרד.
- תוקנה רשימת השפות שמנעה החלה בפועל של בחירת עברית בהגדרות.

## פרקים ומקורות חלופיים

פרקים בעלי אותו מספר עונה ופרק מוצגים כקובייה אחת. גרסאות עם שם עברי/אנגלי או קישור חלופי נשמרות ונגישות דרך **X → אפשרויות → מקורות צפייה**; מספר המקורות מופיע בכרטיס. קישורים חלופיים ומיקומי צפייה אינם נמחקים.

המשך הצפייה בוחר את המקור שנצפה לאחרונה, ומציג את ההתקדמות שלו. הפעלה אוטומטית עוברת לפרק הבא, במקום לעבור לגרסה נוספת של אותו פרק. בלי מספרי פרקים אמינים נשמרים פריטים נפרדים, כדי להימנע מאיחוד שגוי. השינוי חל גם על M3U וגם על Xtream, לפי המטא־נתונים הזמינים.

## EPG וביצועים

- ערוצי Xtream ללא XMLTV מותאם טוענים EPG קצר ברקע לכרטיסים המוצגים, עם עד ארבע בקשות במקביל, מטמון מוגבל והשהיה לפני ניסיון חוזר.
- M3U ו־XMLTV מותאם ממשיכים להשתמש במדריך XMLTV. נתונים חסרים מסומנים במפורש.
- מטמון הספרייה, הצמדת קטגוריות, מועדפי סרטים וסדרות, היסטוריה ומיקומי צפייה מהגרסה הקודמת נשמרים. מטמון VOD ישן מוצג מיד ומתעדכן פעם אחת ברקע כדי לחדש מטא־נתוני תמונות.

## התקנה ועדכון

**Settings → Tools → Check for updates → Update → Restart.**

להתקנה ידנית החליפו את `TsVitch.nro` הקיים, או חלצו `TsVitch-Switch.zip` לשורש הכרטיס. מומלץ להפעיל במצב יישום מלא. מנגנון ההורדה, האימות, ההתקנה והפתיחה מחדש נשמר.

## Validation and limits

Host tests cover Unicode input/editing/search, artwork metadata, grouped episodes with preserved alternate sources, resume source selection, next-episode queues, EPG parsing, actual FriBidi cursor wrappers, and the packaged static font. Existing cache, favorites, pinning, watch progress, catch-up and updater tests remain enabled.

The native UI fixture exercises English M3U and Hebrew Xtream with 500 channels, 4,000 movies and 1,000 series, relative PNG/WebP/SVG artwork, current programme data, duplicate episode variants and controller-accessible Hebrew search. It verifies favorites/pins and cache reuse across reopening and process restart. Release publication is gated by host tests, native UI tests and the Switch cross-build.

No physical Switch was available. Rendering, playback and update/restart behavior on the actual device still require console verification. Artwork and programme availability depend on the provider; unknown episode numbering is never guessed from a translated title alone.
