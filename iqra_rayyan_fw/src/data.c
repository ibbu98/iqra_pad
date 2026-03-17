/*
 * data.c — Surah names and Juz/Surah page mappings
 *          Update SURAH13_PAGE and SURAH15_PAGE once verified from actual books
 */

#include "data.h"

/* ════════════════════════════════════════════════════════════
   13-LINE QURAN
   ════════════════════════════════════════════════════════════ */

const int JUZ13_PAGE[31] = {
    0,  /* unused index 0 */
      1,  14,  28,  42,  56,  70,  84,  98, 112, 126,
    140, 154, 168, 182, 196, 210, 224, 238, 252, 266,
    279, 293, 306, 320, 333, 348, 363, 378, 393, 408
};

const int SURAH13_PAGE[115] = {
    0,  /* unused index 0 */
/*  1 Al-Fatihah    */   1,
/*  2 Al-Baqarah    */   1,
/*  3 Aal-Imran     */  25,
/*  4 An-Nisa       */  39,
/*  5 Al-Ma'idah    */  53,
/*  6 Al-An'am      */  64,
/*  7 Al-A'raf      */  76,
/*  8 Al-Anfal      */  89,
/*  9 At-Tawbah     */  94,
/* 10 Yunus         */ 104,
/* 11 Hud           */ 111,
/* 12 Yusuf         */ 118,
/* 13 Ar-Ra'd       */ 125,
/* 14 Ibrahim       */ 128,
/* 15 Al-Hijr       */ 131,
/* 16 An-Nahl       */ 134,
/* 17 Al-Isra       */ 141,
/* 18 Al-Kahf       */ 147,
/* 19 Maryam        */ 153,
/* 20 Ta-Ha         */ 156,
/* 21 Al-Anbiya     */ 161,
/* 22 Al-Hajj       */ 166,
/* 23 Al-Mu'minun   */ 171,
/* 24 An-Nur        */ 175,
/* 25 Al-Furqan     */ 180,
/* 26 Ash-Shu'ara   */ 184,
/* 27 An-Naml       */ 189,
/* 28 Al-Qasas      */ 193,
/* 29 Al-Ankabut    */ 198,
/* 30 Ar-Rum        */ 202,
/* 31 Luqman        */ 206,
/* 32 As-Sajdah     */ 208,
/* 33 Al-Ahzab      */ 209,
/* 34 Saba          */ 214,
/* 35 Fatir         */ 217,
/* 36 Ya-Sin        */ 220,
/* 37 As-Saffat     */ 223,
/* 38 Sad           */ 227,
/* 39 Az-Zumar      */ 229,
/* 40 Ghafir        */ 234,
/* 41 Fussilat      */ 239,
/* 42 Ash-Shura     */ 242,
/* 43 Az-Zukhruf    */ 245,
/* 44 Ad-Dukhan     */ 248,
/* 45 Al-Jathiyah   */ 250,
/* 46 Al-Ahqaf      */ 251,
/* 47 Muhammad      */ 254,
/* 48 Al-Fath       */ 256,
/* 49 Al-Hujurat    */ 258,
/* 50 Qaf           */ 259,
/* 51 Adh-Dhariyat  */ 260,
/* 52 At-Tur        */ 262,
/* 53 An-Najm       */ 263,
/* 54 Al-Qamar      */ 264,
/* 55 Ar-Rahman     */ 266,
/* 56 Al-Waqi'ah    */ 267,
/* 57 Al-Hadid      */ 269,
/* 58 Al-Mujadila   */ 271,
/* 59 Al-Hashr      */ 273,
/* 60 Al-Mumtahanah */ 275,
/* 61 As-Saff       */ 276,
/* 62 Al-Jumu'ah    */ 277,
/* 63 Al-Munafiqun  */ 277,
/* 64 At-Taghabun   */ 278,
/* 65 At-Talaq      */ 279,
/* 66 At-Tahrim     */ 280,
/* 67 Al-Mulk       */ 281,
/* 68 Al-Qalam      */ 282,
/* 69 Al-Haqqah     */ 283,
/* 70 Al-Ma'arij    */ 284,
/* 71 Nuh           */ 285,
/* 72 Al-Jinn       */ 286,
/* 73 Al-Muzzammil  */ 287,
/* 74 Al-Muddaththir*/ 288,
/* 75 Al-Qiyamah    */ 289,
/* 76 Al-Insan      */ 289,
/* 77 Al-Mursalat   */ 290,
/* 78 An-Naba       */ 291,
/* 79 An-Nazi'at    */ 292,
/* 80 Abasa         */ 293,
/* 81 At-Takwir     */ 293,
/* 82 Al-Infitar    */ 294,
/* 83 Al-Mutaffifin */ 294,
/* 84 Al-Inshiqaq   */ 295,
/* 85 Al-Buruj      */ 295,
/* 86 At-Tariq      */ 296,
/* 87 Al-A'la       */ 296,
/* 88 Al-Ghashiyah  */ 296,
/* 89 Al-Fajr       */ 297,
/* 90 Al-Balad      */ 297,
/* 91 Ash-Shams     */ 298,
/* 92 Al-Layl       */ 298,
/* 93 Ad-Duha       */ 298,
/* 94 Ash-Sharh     */ 298,
/* 95 At-Tin        */ 299,
/* 96 Al-Alaq       */ 299,
/* 97 Al-Qadr       */ 299,
/* 98 Al-Bayyinah   */ 299,
/* 99 Az-Zalzalah   */ 300,
/*100 Al-Adiyat     */ 300,
/*101 Al-Qari'ah    */ 300,
/*102 At-Takathur   */ 300,
/*103 Al-Asr        */ 301,
/*104 Al-Humazah    */ 301,
/*105 Al-Fil        */ 301,
/*106 Quraysh       */ 301,
/*107 Al-Ma'un      */ 301,
/*108 Al-Kawthar    */ 301,
/*109 Al-Kafirun    */ 302,
/*110 An-Nasr       */ 302,
/*111 Al-Masad      */ 302,
/*112 Al-Ikhlas     */ 302,
/*113 Al-Falaq      */ 302,
/*114 An-Nas        */ 302,
};

/* ════════════════════════════════════════════════════════════
   15-LINE QURAN  — TODO: fill in once you verify page numbers
   ════════════════════════════════════════════════════════════ */

const int JUZ15_PAGE[31] = {
    0,  /* unused */
    /* Juz 1-30: update after checking your book */
      1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

const int SURAH15_PAGE[115] = {
    0,  /* unused */
    /* 1-114: update after checking your book */
      1,  1,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,
};

/* ════════════════════════════════════════════════════════════
   SURAH NAMES  (shared by both books)
   ════════════════════════════════════════════════════════════ */
const char *SURAH_NAME[115] = {
    "",  /* unused index 0 */
    "Al-Fatihah",    "Al-Baqarah",    "Aal-Imran",     "An-Nisa",
    "Al-Ma'idah",    "Al-An'am",      "Al-A'raf",      "Al-Anfal",
    "At-Tawbah",     "Yunus",         "Hud",           "Yusuf",
    "Ar-Ra'd",       "Ibrahim",       "Al-Hijr",       "An-Nahl",
    "Al-Isra",       "Al-Kahf",       "Maryam",        "Ta-Ha",
    "Al-Anbiya",     "Al-Hajj",       "Al-Mu'minun",   "An-Nur",
    "Al-Furqan",     "Ash-Shu'ara",   "An-Naml",       "Al-Qasas",
    "Al-Ankabut",    "Ar-Rum",        "Luqman",        "As-Sajdah",
    "Al-Ahzab",      "Saba",          "Fatir",         "Ya-Sin",
    "As-Saffat",     "Sad",           "Az-Zumar",      "Ghafir",
    "Fussilat",      "Ash-Shura",     "Az-Zukhruf",    "Ad-Dukhan",
    "Al-Jathiyah",   "Al-Ahqaf",      "Muhammad",      "Al-Fath",
    "Al-Hujurat",    "Qaf",           "Adh-Dhariyat",  "At-Tur",
    "An-Najm",       "Al-Qamar",      "Ar-Rahman",     "Al-Waqi'ah",
    "Al-Hadid",      "Al-Mujadila",   "Al-Hashr",      "Al-Mumtahanah",
    "As-Saff",       "Al-Jumu'ah",    "Al-Munafiqun",  "At-Taghabun",
    "At-Talaq",      "At-Tahrim",     "Al-Mulk",       "Al-Qalam",
    "Al-Haqqah",     "Al-Ma'arij",    "Nuh",           "Al-Jinn",
    "Al-Muzzammil",  "Al-Muddaththir","Al-Qiyamah",    "Al-Insan",
    "Al-Mursalat",   "An-Naba",       "An-Nazi'at",    "Abasa",
    "At-Takwir",     "Al-Infitar",    "Al-Mutaffifin", "Al-Inshiqaq",
    "Al-Buruj",      "At-Tariq",      "Al-A'la",       "Al-Ghashiyah",
    "Al-Fajr",       "Al-Balad",      "Ash-Shams",     "Al-Layl",
    "Ad-Duha",       "Ash-Sharh",     "At-Tin",        "Al-Alaq",
    "Al-Qadr",       "Al-Bayyinah",   "Az-Zalzalah",   "Al-Adiyat",
    "Al-Qari'ah",    "At-Takathur",   "Al-Asr",        "Al-Humazah",
    "Al-Fil",        "Quraysh",       "Al-Ma'un",      "Al-Kawthar",
    "Al-Kafirun",    "An-Nasr",       "Al-Masad",      "Al-Ikhlas",
    "Al-Falaq",      "An-Nas",
};
