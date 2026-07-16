/*
** SQLite FTS5 Arabic Tokenizer with Transliteration and Fuzzy Matching
**
** Based on GreentechApps/sqlite3-arabic-tokenizer approach
** Combined with streetwriters/sqlite-better-trigram
**
** Version 0.1.0 — token-format revision (see README):
**   * The primary token (flags=0) is always the literal word:
**       Arabic  -> diacritic-stripped word (unchanged from 0.0.x)
**       other   -> ASCII-lowercased word   (unchanged from 0.0.x)
**   * All fuzzy tokens are emitted as FTS5 colocated synonyms (flags=1):
**       Arabic       -> character trigrams (unchanged) + transliteration
**                       + normalized transliteration
**       ASCII/Latin  -> normalized form (see normalize_latin_ascii)
**       CJK          -> character bigrams + trigrams
**       other script -> character trigrams
**   * transliterate no longer requires diacritics: every Arabic word emits
**     its transliteration when enabled.
**   * CJK punctuation and fullwidth punctuation are word separators.
**   * generate_phonetic (spellfix hash) is retired: the argument is accepted
**     and ignored for schema compatibility. It collided 41-76% of the Latin
**     vocabulary per language (e.g. "sayyiban" == "subhan"), producing
**     unrelated search results. normalize_latin_ascii replaces it (1-9%).
**
** Arguments (all optional, shown with defaults):
**   remove_diacritics 1    strip Arabic diacritics; normalize alif/yeh/teh
**   generate_trigrams 1    colocated char n-grams (Arabic/Indic: 3; CJK: 2+3)
**   transliterate 1        colocated Latin transliteration of Arabic words
**   generate_normalized 1  colocated normalized Latin token
**   case_sensitive 0       keep ASCII case (fuzzy tokens are always lowered)
**   generate_phonetic -    DEPRECATED, ignored
*/

#include "sqlite3ext.h"

SQLITE_EXTENSION_INIT1

/* Reported by SELECT arabic_phonetic_fuzzy_trigram_version().
** Bump on ANY change to token output — the backend index and the on-device
** queries must be produced by the same token format. */
#define ARABIC_PHONETIC_FUZZY_TRIGRAM_VERSION "0.2.0"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* FTS5 tokenize-reason flags (mirrors fts5.h; defined here so the amalgamation
** headers alone are enough to build). Currently informational only — emission
** is symmetric for documents and queries. */
#ifndef FTS5_TOKENIZE_QUERY
#define FTS5_TOKENIZE_QUERY 0x0001
#endif
#ifndef FTS5_TOKEN_COLOCATED
#define FTS5_TOKEN_COLOCATED 0x0001
#endif

/* Forward declarations */
typedef struct fts5_api fts5_api;
typedef struct fts5_tokenizer fts5_tokenizer;
typedef struct Fts5Tokenizer Fts5Tokenizer;

typedef struct arabic_phonetic_fuzzy_trigram_tokenizer arabic_phonetic_fuzzy_trigram_tokenizer;

/*
** Tokenizer instance structure
*/
struct arabic_phonetic_fuzzy_trigram_tokenizer {
    int bRemoveDiacritics;    /* Remove Arabic diacritics */
    int bGenerateTrigrams;    /* Generate colocated char n-gram tokens */
    int bTransliterate;       /* Generate colocated transliterated tokens */
    int bGenerateNormalized;  /* Generate colocated normalized Latin tokens */
    int bCaseSensitive;       /* Case sensitive primary tokens */
};

static char *aliff = "ا";
static char *r1 = "ى";
static char *r2 = "ئ";
static char *r3 = "ة";

/*
** Classify an Arabic codepoint for remove_diacritic().
** Return values keep the semantics of the historical 67-entry index table
** (replaced by range checks — this runs for every codepoint of every
** Arabic word, including during on-device FTS rebuilds):
**   -1      no replacement (copy through)
**   < 59    diacritic (strip)
**   59..63  alif variant  -> ا
**   64      yeh           -> ى
**   65      hamza         -> ئ
**   66      heh           -> ة
*/
static int unicode_diacritic(int c) {
    if (c == 1548) return 0;                 /* arabic comma */
    if (c >= 1552 && c <= 1562) return 1;    /* small high signs */
    if (c >= 1750 && c <= 1773) return 12;   /* quranic annotation signs */
    if (c == 1600) return 36;                /* tatweel */
    if (c >= 1611 && c <= 1631) return 37;   /* tashkeel */
    if (c == 1648) return 58;                /* superscript alef */
    if (c == 1570 || c == 1571 || c == 1573 || c == 1649 || c == 1671) return 59;
    if (c == 1610) return 64;                /* yeh */
    if (c == 1569) return 65;                /* hamza */
    if (c == 1607) return 66;                /* heh */
    return -1;
}

/*
** UTF-8 codepoint extraction, bounds-safe.
**
** max_bytes is the number of bytes remaining in the buffer from `text`.
** The function never reads past text[max_bytes-1] — a truncated multibyte
** sequence at the end of a value is reported as invalid (-1, consume 1)
** instead of over-reading. On invalid input *bytes_consumed is 1 so callers
** always make progress.
*/
static int get_unicode_codepoint(const char *text, int max_bytes, int *bytes_consumed) {
    const unsigned char *utf8 = (const unsigned char *) text;
    int codepoint = 0;

    *bytes_consumed = 1;
    if (max_bytes <= 0) return -1;

    /* Single byte ASCII */
    if ((utf8[0] & 0x80) == 0) {
        codepoint = utf8[0];
    }
        /* Two byte sequence (110xxxxx 10xxxxxx) */
    else if ((utf8[0] & 0xE0) == 0xC0) {
        if (max_bytes < 2 || (utf8[1] & 0xC0) != 0x80) {
            return -1; /* Invalid or truncated sequence */
        }
        codepoint = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
        *bytes_consumed = 2;
    }
        /* Three byte sequence (1110xxxx 10xxxxxx 10xxxxxx) */
    else if ((utf8[0] & 0xF0) == 0xE0) {
        if (max_bytes < 3 || (utf8[1] & 0xC0) != 0x80 || (utf8[2] & 0xC0) != 0x80) {
            return -1; /* Invalid or truncated sequence */
        }
        codepoint = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
        *bytes_consumed = 3;
    }
        /* Four byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx) */
    else if ((utf8[0] & 0xF8) == 0xF0) {
        if (max_bytes < 4 || (utf8[1] & 0xC0) != 0x80 || (utf8[2] & 0xC0) != 0x80 ||
            (utf8[3] & 0xC0) != 0x80) {
            return -1; /* Invalid or truncated sequence */
        }
        codepoint = ((utf8[0] & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) |
                    ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
        *bytes_consumed = 4;
    } else {
        return -1; /* Invalid UTF-8 start byte */
    }

    /* Check for overlong encoding or invalid codepoints */
    if ((*bytes_consumed == 2 && codepoint < 0x80) ||
        (*bytes_consumed == 3 && codepoint < 0x800) ||
        (*bytes_consumed == 4 && codepoint < 0x10000) ||
        codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        *bytes_consumed = 1;
        return -1; /* Invalid codepoint */
    }

    return codepoint;
}


/*
** Check if a single Unicode codepoint is Arabic
** Returns 1 if Arabic, 0 otherwise
*/
static int is_arabic_char(int codepoint) {
    return ((codepoint >= 0x0600 && codepoint <= 0x06FF) ||  /* Arabic */
            (codepoint >= 0x0750 && codepoint <= 0x077F) ||  /* Arabic Supplement */
            (codepoint >= 0x08A0 && codepoint <= 0x08FF) ||  /* Arabic Extended-A */
            (codepoint >= 0xFB50 && codepoint <= 0xFDFF) ||  /* Arabic Presentation Forms-A */
            (codepoint >= 0xFE70 && codepoint <= 0xFEFF));   /* Arabic Presentation Forms-B */
}

/*
** Check if a codepoint belongs to a CJK script (Han, Kana).
** CJK text has no word separators, so it needs bigram indexing:
** most Chinese words are two characters.
*/
static int is_cjk_char(int codepoint) {
    return ((codepoint >= 0x3040 && codepoint <= 0x30FF) ||  /* Hiragana + Katakana */
            (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||  /* CJK Extension A */
            (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||  /* CJK Unified */
            (codepoint >= 0xF900 && codepoint <= 0xFAFF));   /* CJK Compatibility */
}

/*
** Convert ASCII characters to lowercase in-place
** Only affects A-Z, leaves other characters unchanged
*/
static void lowercase_ascii(char *str, int len) {
    for (int i = 0; i < len; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] + 32;  // Convert uppercase to lowercase
        }
    }
}

/*
** Check if a word is Arabic by checking the first character
** Returns 1 if the first character is Arabic, 0 otherwise
*/
static int is_arabic_word(const char *text, int text_len) {
    if (!text || text_len <= 0) {
        return 0;
    }

    int bytes_consumed;
    int first_codepoint = get_unicode_codepoint(text, text_len, &bytes_consumed);

    if (first_codepoint == -1) {
        return 0;
    }

    return is_arabic_char(first_codepoint);
}

static int is_all_ascii(const char *text, int text_len) {
    for (int i = 0; i < text_len; i++) {
        if ((unsigned char) text[i] & 0x80) return 0;
    }
    return 1;
}

static int contains_cjk(const char *text, int text_len) {
    int pos = 0;
    while (pos < text_len) {
        int bytes;
        int cp = get_unicode_codepoint(text + pos, text_len - pos, &bytes);
        if (cp != -1 && is_cjk_char(cp)) return 1;
        pos += (bytes > 0) ? bytes : 1;
    }
    return 0;
}

static char *remove_diacritic(const char *text, int input_len, int *output_len) {
    if (!text || input_len <= 0) {
        *output_len = 0;
        return NULL;
    }

    char *replaced = (char *) sqlite3_malloc(input_len + 5);
    if (!replaced) {
        *output_len = 0;
        return NULL;
    }

    int j = 0;
    int i = 0;

    while (i < input_len) {
        int l;
        int z = get_unicode_codepoint(text + i, input_len - i, &l);

        if (z < 0) {
            /* Invalid/truncated byte: copy through, keep making progress */
            replaced[j++] = text[i];
            i++;
            continue;
        }

        /* Every replaceable codepoint is >= U+0600; skip the classifier
        ** entirely for ASCII and Latin. */
        int index = (z >= 0x0600) ? unicode_diacritic(z) : -1;

        if (index == -1) {
            for (int k = 0; k < l; k++) {
                replaced[j++] = text[i + k];
            }
        } else if (index >= 59 && index <= 63) {
            replaced[j++] = aliff[0];
            replaced[j++] = aliff[1];
        } else if (index == 64) {
            replaced[j++] = r1[0];
            replaced[j++] = r1[1];
        } else if (index == 65) {
            replaced[j++] = r2[0];
            replaced[j++] = r2[1];
        } else if (index == 66) {
            replaced[j++] = r3[0];
            replaced[j++] = r3[1];
        }
        /* index < 59: diacritic, skip (don't add to output) */
        i += l;
    }

    replaced[j] = '\0';
    *output_len = j;
    return replaced;
}

typedef struct translit_pair {
    int unicode;
    const char *ascii;
} translit_pair;

/* Sorted by codepoint — looked up via translit_lookup() binary search. */
static const translit_pair arabic_translit_table[] = {
        /* Arabic letters */
        {0x0621, ""},     /* HAMZA ء */
        {0x0622, "aa"},   /* ALEF_WITH_MADDA_ABOVE آ */
        {0x0623, "a"},    /* ALEF_WITH_HAMZA_ABOVE أ */
        {0x0624, "u"},    /* WAW_WITH_HAMZA_ABOVE ؤ */
        {0x0625, "i"},    /* ALEF_WITH_HAMZA_BELOW إ */
        {0x0626, "y"},    /* YEH_WITH_HAMZA_ABOVE ئ */
        {0x0627, "a"},    /* ALEF ا */
        {0x0628, "b"},    /* BEH ب */
        {0x0629, "h"},    /* TEH_MARBUTA ة */
        {0x062A, "t"},    /* TEH ت */
        {0x062B, "th"},   /* THEH ث */
        {0x062C, "j"},    /* JEEM ج */
        {0x062D, "h"},    /* HAH ح */
        {0x062E, "kh"},   /* KHAH خ */
        {0x062F, "d"},    /* DAL د */
        {0x0630, "dh"},   /* THAL ذ */
        {0x0631, "r"},    /* REH ر */
        {0x0632, "z"},    /* ZAIN ز */
        {0x0633, "s"},    /* SEEN س */
        {0x0634, "sh"},   /* SHEEN ش */
        {0x0635, "s"},    /* SAD ص */
        {0x0636, "d"},    /* DAD ض */
        {0x0637, "t"},    /* TAH ط */
        {0x0638, "z"},    /* ZAH ظ */
        {0x0639, "a"},    /* AIN ع */
        {0x063A, "gh"},   /* GHAIN غ */
        {0x0641, "f"},    /* FEH ف */
        {0x0642, "q"},    /* QAF ق */
        {0x0643, "k"},    /* KAF ك */
        {0x0644, "l"},    /* LAM ل */
        {0x0645, "m"},    /* MEEM م */
        {0x0646, "n"},    /* NOON ن */
        {0x0647, "h"},    /* HEH ه */
        {0x0648, "w"},    /* WAW و */
        {0x0649, "a"},    /* ALEF_MAKSURA ى */
        {0x064A, "y"},    /* YEH ي */
        {0x064B, "an"},   /* FATHATAN ً */
        {0x064C, "un"},   /* DAMMATAN ٌ */
        {0x064D, "in"},   /* KASRATAN ٍ */
        {0x064E, "a"},    /* FATHA َ */
        {0x064F, "u"},    /* DAMMA ُ */
        {0x0650, "i"},    /* KASRA ِ */
        {0x0651, ""},     /* SHADDA ّ - handled separately */
        {0x0652, ""},     /* SUKUN ْ */
        {0x0671, "a"},    /* ALEF_WASLA ٱ */
        /* Persian / Urdu letters (fa, ps, ur content and keyboards) */
        {0x067E, "p"},    /* PEH پ */
        {0x0686, "ch"},   /* TCHEH چ */
        {0x0698, "zh"},   /* JEH ژ */
        {0x06A9, "k"},    /* KEHEH ک */
        {0x06AF, "g"},    /* GAF گ */
        {0x06BA, "n"},    /* NOON GHUNNA ں */
        {0x06BE, "h"},    /* HEH DOACHASHMEE ھ */
        {0x06C1, "h"},    /* HEH GOAL ہ */
        {0x06C3, "h"},    /* TEH MARBUTA GOAL ۃ */
        {0x06CC, "y"},    /* FARSI YEH ی */
        {0x06D2, "e"},    /* YEH BARREE ے */
        {0,      NULL}
};

/* Number of real entries in a translit table (excluding the {0, NULL} end). */
#define TRANSLIT_COUNT(tab) ((int) (sizeof(tab) / sizeof((tab)[0]) - 1))

/*
** Binary search over a codepoint-sorted translit table.
** Replaces per-character linear scans (the Latin table has ~190 entries and
** runs for every character during DB generation and on-device FTS rebuilds).
*/
static const char *translit_lookup(const translit_pair *tab, int n, int cp) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (tab[mid].unicode == cp) return tab[mid].ascii;
        if (tab[mid].unicode < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}


static char *transliterate_arabic_text(const char *input, int input_len, int *output_len) {
    char *output = sqlite3_malloc(input_len * 4 + 1);
    if (!output) {
        *output_len = 0;
        return NULL;
    }

    int input_pos = 0;
    int output_pos = 0;
    int prev_codepoint = 0;

    while (input_pos < input_len) {
        int bytes_consumed;
        int codepoint = get_unicode_codepoint(input + input_pos, input_len - input_pos, &bytes_consumed);

        if (codepoint == -1) {
            input_pos++;
            continue;
        }

        /* Preserve spaces */
        if (codepoint == 0x0020) {
            output[output_pos++] = ' ';
            input_pos += bytes_consumed;
            prev_codepoint = codepoint;
            continue;
        }

        /* Handle shadda (gemination) - double previous consonant */
        if (codepoint == 0x0651 && prev_codepoint != 0) { /* ّ */
            const char *geminate = translit_lookup(
                    arabic_translit_table, TRANSLIT_COUNT(arabic_translit_table),
                    prev_codepoint);
            if (geminate) {
                int len = strlen(geminate);
                if (output_pos + len < input_len * 4) {
                    memcpy(output + output_pos, geminate, len);
                    output_pos += len;
                }
            }
            input_pos += bytes_consumed;
            continue;
        }

        /* Look up transliteration */
        const char *translit = translit_lookup(
                arabic_translit_table, TRANSLIT_COUNT(arabic_translit_table),
                codepoint);

        if (translit && strlen(translit) > 0) {
            int len = strlen(translit);
            if (output_pos + len < input_len * 4) {
                memcpy(output + output_pos, translit, len);
                output_pos += len;
            }
        } else if (codepoint < 128 && isprint(codepoint)) {
            /* Keep ASCII as-is */
            if (output_pos < input_len * 4) {
                output[output_pos++] = (char) codepoint;
            }
        }

        prev_codepoint = codepoint;
        input_pos += bytes_consumed;
    }

    output[output_pos] = '\0';
    *output_len = output_pos;
    return output;
}


/*
** Transliterate text using the table
*/
static char *transliterate_text(const char *input, int input_len, int *output_len) {
    /* Check if text is Arabic */
    if (is_arabic_word(input, input_len)) {
        return transliterate_arabic_text(input, input_len, output_len);
    }

    /* Fallback for non-Arabic text with comprehensive Latin diacritics */
    char *output = sqlite3_malloc(input_len * 3 + 1);
    if (!output) {
        *output_len = 0;
        return NULL;
    }

    int input_pos = 0;
    int output_pos = 0;

    while (input_pos < input_len) {
        int bytes_consumed;
        int codepoint = get_unicode_codepoint(input + input_pos, input_len - input_pos, &bytes_consumed);

        if (codepoint == -1) {
            input_pos++;
            continue;
        }

        /* Preserve spaces */
        if (codepoint == 0x0020) {
            output[output_pos++] = ' ';
            input_pos += bytes_consumed;
            continue;
        }

        /* Latin diacritics, sorted by codepoint for translit_lookup() */
        static const translit_pair latin_table[] = {
                {0x00C0, "A"},
                {0x00C1, "A"},
                {0x00C2, "A"},
                {0x00C3, "A"},
                {0x00C4, "A"},
                {0x00C5, "A"},
                {0x00C6, "AE"},
                {0x00C7, "C"},
                {0x00C8, "E"},
                {0x00C9, "E"},
                {0x00CA, "E"},
                {0x00CB, "E"},
                {0x00CC, "I"},
                {0x00CD, "I"},
                {0x00CE, "I"},
                {0x00CF, "I"},
                {0x00D0, "D"},
                {0x00D1, "N"},
                {0x00D2, "O"},
                {0x00D3, "O"},
                {0x00D4, "O"},
                {0x00D5, "O"},
                {0x00D6, "O"},
                {0x00D8, "O"},
                {0x00D9, "U"},
                {0x00DA, "U"},
                {0x00DB, "U"},
                {0x00DC, "U"},
                {0x00DD, "Y"},
                {0x00DE, "TH"},
                {0x00DF, "ss"},
                {0x00E0, "a"},
                {0x00E1, "a"},
                {0x00E2, "a"},
                {0x00E3, "a"},
                {0x00E4, "a"},
                {0x00E5, "a"},
                {0x00E6, "ae"},
                {0x00E7, "c"},
                {0x00E8, "e"},
                {0x00E9, "e"},
                {0x00EA, "e"},
                {0x00EB, "e"},
                {0x00EC, "i"},
                {0x00ED, "i"},
                {0x00EE, "i"},
                {0x00EF, "i"},
                {0x00F0, "d"},
                {0x00F1, "n"},
                {0x00F2, "o"},
                {0x00F3, "o"},
                {0x00F4, "o"},
                {0x00F5, "o"},
                {0x00F6, "o"},
                {0x00F8, "o"},
                {0x00F9, "u"},
                {0x00FA, "u"},
                {0x00FB, "u"},
                {0x00FC, "u"},
                {0x00FD, "y"},
                {0x00FE, "th"},
                {0x00FF, "y"},
                {0x0100, "A"},
                {0x0101, "a"},
                {0x0102, "A"},
                {0x0103, "a"},
                {0x0104, "A"},
                {0x0105, "a"},
                {0x0106, "C"},
                {0x0107, "c"},
                {0x0108, "C"},
                {0x0109, "c"},
                {0x010A, "C"},
                {0x010B, "c"},
                {0x010C, "C"},
                {0x010D, "c"},
                {0x010E, "D"},
                {0x010F, "d"},
                {0x0110, "D"},
                {0x0111, "d"},
                {0x0112, "E"},
                {0x0113, "e"},
                {0x0114, "E"},
                {0x0115, "e"},
                {0x0116, "E"},
                {0x0117, "e"},
                {0x0118, "E"},
                {0x0119, "e"},
                {0x011A, "E"},
                {0x011B, "e"},
                {0x011C, "G"},
                {0x011D, "g"},
                {0x011E, "G"},
                {0x011F, "g"},
                {0x0120, "G"},
                {0x0121, "g"},
                {0x0122, "G"},
                {0x0123, "g"},
                {0x0124, "H"},
                {0x0125, "h"},
                {0x0126, "H"},
                {0x0127, "h"},
                {0x0128, "I"},
                {0x0129, "i"},
                {0x012A, "I"},
                {0x012B, "i"},
                {0x012C, "I"},
                {0x012D, "i"},
                {0x012E, "I"},
                {0x012F, "i"},
                {0x0130, "I"},
                {0x0131, "i"},
                {0x0134, "J"},
                {0x0135, "j"},
                {0x0136, "K"},
                {0x0137, "k"},
                {0x0139, "L"},
                {0x013A, "l"},
                {0x013B, "L"},
                {0x013C, "l"},
                {0x013D, "L"},
                {0x013E, "l"},
                {0x013F, "L"},
                {0x0140, "l"},
                {0x0141, "L"},
                {0x0142, "l"},
                {0x0143, "N"},
                {0x0144, "n"},
                {0x0145, "N"},
                {0x0146, "n"},
                {0x0147, "N"},
                {0x0148, "n"},
                {0x014A, "NG"},
                {0x014B, "ng"},
                {0x014C, "O"},
                {0x014D, "o"},
                {0x014E, "O"},
                {0x014F, "o"},
                {0x0150, "O"},
                {0x0151, "o"},
                {0x0152, "OE"},
                {0x0153, "oe"},
                {0x0154, "R"},
                {0x0155, "r"},
                {0x0156, "R"},
                {0x0157, "r"},
                {0x0158, "R"},
                {0x0159, "r"},
                {0x015A, "S"},
                {0x015B, "s"},
                {0x015C, "S"},
                {0x015D, "s"},
                {0x015E, "S"},
                {0x015F, "s"},
                {0x0160, "S"},
                {0x0161, "s"},
                {0x0162, "T"},
                {0x0163, "t"},
                {0x0164, "T"},
                {0x0165, "t"},
                {0x0166, "T"},
                {0x0167, "t"},
                {0x0168, "U"},
                {0x0169, "u"},
                {0x016A, "U"},
                {0x016B, "u"},
                {0x016C, "U"},
                {0x016D, "u"},
                {0x016E, "U"},
                {0x016F, "u"},
                {0x0170, "U"},
                {0x0171, "u"},
                {0x0172, "U"},
                {0x0173, "u"},
                {0x0174, "W"},
                {0x0175, "w"},
                {0x0176, "Y"},
                {0x0177, "y"},
                {0x0178, "Y"},
                {0x0179, "Z"},
                {0x017A, "z"},
                {0x017B, "Z"},
                {0x017C, "z"},
                {0x017D, "Z"},
                {0x017E, "z"},
                /* Dotted consonants used in scholarly Arabic transliteration
                ** (ḍ ḥ ḳ ṭ ẓ …) — without these the letter is dropped
                ** entirely and e.g. ʿAẓīm folds to "aim" instead of "azim". */
                {0x1E0C, "D"},
                {0x1E0D, "d"},
                {0x1E24, "H"},
                {0x1E25, "h"},
                {0x1E32, "K"},
                {0x1E33, "k"},
                {0x1E62, "S"},
                {0x1E63, "s"},
                {0x1E6C, "T"},
                {0x1E6D, "t"},
                {0x1E92, "Z"},
                {0x1E93, "z"},
                {0x1E96, "h"},
                {0,      NULL}
        };

        const char *translit = translit_lookup(
                latin_table, TRANSLIT_COUNT(latin_table), codepoint);

        if (translit) {
            int len = strlen(translit);
            memcpy(output + output_pos, translit, len);
            output_pos += len;
        } else if (codepoint < 128 && isprint(codepoint)) {
            output[output_pos++] = (char) codepoint;
        }

        input_pos += bytes_consumed;
    }

    output[output_pos] = '\0';
    *output_len = output_pos;
    return output;
}

/*
** Check if character is a word separator
*/
static int is_word_separator(int codepoint) {
    /* ASCII whitespace and punctuation */
    if (codepoint < 128) {
        return isspace(codepoint) || ispunct(codepoint);
    }

    /* Common Unicode whitespace */
    if (codepoint == 0x00A0) return 1; /* Non-breaking space */
    if (codepoint >= 0x2000 && codepoint <= 0x200B) return 1; /* En quad to zero width space */
    if (codepoint >= 0x2028 && codepoint <= 0x2029) return 1; /* Line/paragraph separator */
    if (codepoint == 0x202F) return 1; /* Narrow no-break space */
    if (codepoint == 0x205F) return 1; /* Medium mathematical space */
    if (codepoint == 0x3000) return 1; /* Ideographic space */

    /* Arabic punctuation */
    if (codepoint >= 0x060C && codepoint <= 0x061F) return 1; /* Arabic comma to question mark */
    if (codepoint == 0x06D4) return 1; /* Arabic full stop */

    /* Arabic-specific separators */
    if (codepoint == 0x0020) return 1; /* Regular space */
    if (codepoint == 0x200C) return 1; /* Zero-width non-joiner */
    if (codepoint == 0x200D) return 1; /* Zero-width joiner */

    /* General punctuation */
    if (codepoint >= 0x2010 && codepoint <= 0x2027) return 1;
    if (codepoint >= 0x2030 && codepoint <= 0x205E) return 1;

    /* CJK punctuation (、。〈〉「」…) — without these, unsegmented Chinese
    ** text fuses into single tokens spanning whole paragraphs (observed:
    ** 522-char tokens). 0x3005 々 (iteration mark) and 0x3007 〇
    ** (ideographic zero) are word-forming and stay part of tokens. */
    if (codepoint >= 0x3001 && codepoint <= 0x303F &&
        codepoint != 0x3005 && codepoint != 0x3007) return 1;

    /* Fullwidth ASCII punctuation （！？：…）; fullwidth alphanumerics are
    ** intentionally not separators. */
    if (codepoint >= 0xFF01 && codepoint <= 0xFF0F) return 1;
    if (codepoint >= 0xFF1A && codepoint <= 0xFF20) return 1;
    if (codepoint >= 0xFF3B && codepoint <= 0xFF40) return 1;
    if (codepoint >= 0xFF5B && codepoint <= 0xFF65) return 1;

    return 0;
}

/*
** Number of UTF-8 characters in a byte range.
*/
static int utf8_char_count(const char *word, int word_len) {
    int pos = 0, count = 0;
    while (pos < word_len) {
        int bytes;
        get_unicode_codepoint(word + pos, word_len - pos, &bytes);
        pos += (bytes > 0) ? bytes : 1;
        count++;
    }
    return count;
}

/*
** Emit character n-grams (n = nmin..nmax) as colocated tokens.
** Works on UTF-8 characters, not bytes, and handles arbitrarily long words
** (CJK clause runs) with a small sliding window of character offsets.
*/
static int emit_ngrams_unicode(const char *word, int word_len, int nmin, int nmax,
                               void *pCtx, int start, int end,
                               int (*xToken)(void *, int, const char *, int, int, int)) {
    int ring[4];       /* byte offsets of the last <=4 char starts (nmax <= 3) */
    int char_count = 0;
    int pos = 0;
    int rc = SQLITE_OK;

    if (nmax > 3) nmax = 3;
    if (nmin < 2) nmin = 2;

    while (pos < word_len && rc == SQLITE_OK) {
        int bytes;
        get_unicode_codepoint(word + pos, word_len - pos, &bytes);
        if (bytes < 1) bytes = 1;
        ring[char_count & 3] = pos;
        char_count++;
        pos += bytes;  /* pos is now the byte end of character (char_count-1) */

        for (int n = nmin; n <= nmax && rc == SQLITE_OK; n++) {
            if (char_count >= n) {
                int from = ring[(char_count - n) & 3];
                rc = xToken(pCtx, FTS5_TOKEN_COLOCATED, word + from, pos - from, start, end);
            }
        }
    }
    return rc;
}

/*
** Emit trigrams for an Arabic word — byte-compatible with the 0.0.x output:
** overlapping 3-character grams; words shorter than 3 characters are emitted
** whole (as a colocated duplicate of the primary), exactly as before.
*/
static int emit_trigrams_arabic_compat(const char *word, int word_len,
                                       void *pCtx, int start, int end,
                                       int (*xToken)(void *, int, const char *, int, int, int)) {
    if (utf8_char_count(word, word_len) < 3) {
        return xToken(pCtx, FTS5_TOKEN_COLOCATED, word, word_len, start, end);
    }
    return emit_ngrams_unicode(word, word_len, 3, 3, pCtx, start, end, xToken);
}

/*
** Normalize a Latin/ASCII token into its fuzzy-match form.
**
** MUST stay byte-identical to normalize_latin() in the backend's
** tools/search-qa/qa_common.py — both sides of the index depend on it.
** Rules: lowercase; drop apostrophes and hyphens; map ee->i, oo->u, ou->u,
** q->k; collapse runs of the same character.
**
** Chosen over the retired spellfix phonetic hash: measured on the full
** 17-language fleet vocabulary it collides 1-9% of words (vs 41-76%), and
** the collisions are variant spellings of the same word (salaam/salam).
** Output length is always <= input length. Returns the output length.
*/
static int normalize_latin_ascii(const char *in, int n, char *out) {
    int i = 0, o = 0;

    while (i < n) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c += 32;

        if (c == '\'' || c == '-') {
            i++;
            continue;
        }

        char next = 0;
        if (i + 1 < n) {
            next = in[i + 1];
            if (next >= 'A' && next <= 'Z') next += 32;
        }

        if (c == 'e' && next == 'e') { c = 'i'; i += 2; }
        else if (c == 'o' && next == 'o') { c = 'u'; i += 2; }
        else if (c == 'o' && next == 'u') { c = 'u'; i += 2; }
        else if (c == 'q') { c = 'k'; i += 1; }
        else { i += 1; }

        if (o == 0 || out[o - 1] != c) {  /* collapse repeats */
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return o;
}

/*
** Emit the normalized form of an ASCII token as a colocated synonym,
** skipping the emission when normalization is a no-op.
*/
static int emit_normalized(const char *token, int token_len,
                           void *pCtx, int start, int end,
                           int (*xToken)(void *, int, const char *, int, int, int)) {
    int rc = SQLITE_OK;
    char *norm = sqlite3_malloc(token_len + 1);
    if (!norm) return SQLITE_NOMEM;

    int norm_len = normalize_latin_ascii(token, token_len, norm);
    if (norm_len > 0 &&
        (norm_len != token_len || memcmp(token, norm, token_len) != 0)) {
        rc = xToken(pCtx, FTS5_TOKEN_COLOCATED, norm, norm_len, start, end);
    }
    sqlite3_free(norm);
    return rc;
}

/*
** Canonical form of Persian/Urdu letter variants and Arabic-Indic digits.
**
** Quranic/Arabic content uses Arabic codepoints while fa/ps/ur keyboards and
** translations use Persian/Urdu variants — visually near-identical, different
** bytes. Measured on the production DBs: 65% of Urdu, 32% of Farsi and 17%
** of Pashto words contain variant codepoints, so cross-variant queries miss
** without this mapping. Letter targets are the canonical classes that
** remove_diacritic() already produces (yeh -> ى, heh -> ة).
**
** Returns the canonical codepoint, or the input codepoint unchanged.
*/
static int arabic_variant_canonical(int cp) {
    switch (cp) {
        case 0x06A9: return 0x0643;  /* ک keheh          -> ك kaf */
        case 0x06CC: return 0x0649;  /* ی farsi yeh      -> ى (yeh class) */
        case 0x06D2: return 0x0649;  /* ے yeh barree     -> ى */
        case 0x06C1: return 0x0629;  /* ہ heh goal       -> ة (heh class) */
        case 0x06BE: return 0x0629;  /* ھ heh doachashmee-> ة */
        case 0x06C3: return 0x0629;  /* ۃ teh marbuta goal -> ة */
        case 0x06BA: return 0x0646;  /* ں noon ghunna    -> ن noon */
        default: break;
    }
    if (cp >= 0x0660 && cp <= 0x0669) return '0' + (cp - 0x0660);  /* ٠-٩ */
    if (cp >= 0x06F0 && cp <= 0x06F9) return '0' + (cp - 0x06F0);  /* ۰-۹ */
    return cp;
}

/*
** Rewrite an Arabic-script word with variant letters/digits canonicalized.
** Returns an sqlite3_malloc'd string when something changed, NULL when the
** word is already canonical (the common case for pure Arabic content).
** Canonical output is never longer than the input: 2-byte variants map to
** 2-byte letters or 1-byte ASCII digits.
*/
static char *arabic_canonicalize(const char *in, int len, int *out_len) {
    char *out = sqlite3_malloc(len + 1);
    int changed = 0, i = 0, o = 0;

    if (!out) {
        *out_len = 0;
        return NULL;
    }

    while (i < len) {
        int l;
        int cp = get_unicode_codepoint(in + i, len - i, &l);
        if (cp < 0) {
            out[o++] = in[i];
            i++;
            continue;
        }

        int canon = arabic_variant_canonical(cp);
        if (canon == cp) {
            for (int k = 0; k < l; k++) out[o++] = in[i + k];
        } else if (canon < 0x80) {
            changed = 1;
            out[o++] = (char) canon;
        } else {
            /* All canonical letter targets are 2-byte codepoints (< U+0800) */
            changed = 1;
            out[o++] = (char) (0xC0 | (canon >> 6));
            out[o++] = (char) (0x80 | (canon & 0x3F));
        }
        i += l;
    }

    if (!changed) {
        sqlite3_free(out);
        *out_len = 0;
        return NULL;
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}

/*
** FTS5 tokenizer interface implementation
*/

/*
** Create a new tokenizer instance
*/
static int arabic_phonetic_fuzzy_trigram_create(
        void *pUnused,
        const char **azArg, int nArg,
        Fts5Tokenizer **ppOut
) {
    arabic_phonetic_fuzzy_trigram_tokenizer *pNew;
    int i;

    (void) pUnused;

    pNew = sqlite3_malloc(sizeof(arabic_phonetic_fuzzy_trigram_tokenizer));
    if (pNew == NULL) return SQLITE_NOMEM;

    memset(pNew, 0, sizeof(arabic_phonetic_fuzzy_trigram_tokenizer));

    /* Default settings */
    pNew->bRemoveDiacritics = 1;
    pNew->bGenerateTrigrams = 1;
    pNew->bTransliterate = 1;
    pNew->bGenerateNormalized = 1;
    pNew->bCaseSensitive = 0;

    /* Parse arguments */
    for (i = 0; i < nArg; i++) {
        if (strcmp(azArg[i], "remove_diacritics") == 0 && i + 1 < nArg) {
            pNew->bRemoveDiacritics = atoi(azArg[i + 1]);
            i++;
        } else if (strcmp(azArg[i], "generate_trigrams") == 0 && i + 1 < nArg) {
            pNew->bGenerateTrigrams = atoi(azArg[i + 1]);
            i++;
        } else if (strcmp(azArg[i], "transliterate") == 0 && i + 1 < nArg) {
            pNew->bTransliterate = atoi(azArg[i + 1]);
            i++;
        } else if (strcmp(azArg[i], "generate_normalized") == 0 && i + 1 < nArg) {
            pNew->bGenerateNormalized = atoi(azArg[i + 1]);
            i++;
        } else if (strcmp(azArg[i], "generate_phonetic") == 0 && i + 1 < nArg) {
            /* DEPRECATED and ignored. The spellfix-derived hash collided
            ** unrelated words across languages (41-76% of vocabulary); it is
            ** superseded by generate_normalized. The argument is still
            ** accepted because existing DB schemas contain it. */
            i++;
        } else if (strcmp(azArg[i], "case_sensitive") == 0 && i + 1 < nArg) {
            pNew->bCaseSensitive = atoi(azArg[i + 1]);
            i++;
        }
    }

    *ppOut = (Fts5Tokenizer *) pNew;
    return SQLITE_OK;
}

/*
** Delete a tokenizer instance
*/
static void arabic_phonetic_fuzzy_trigram_delete(Fts5Tokenizer *pTokenizer) {
    sqlite3_free(pTokenizer);
}

/*
** Process a single word: emit its primary token and colocated fuzzy tokens.
**
** Primary-token contract (compatibility across tokenizer versions):
**   Arabic word  -> diacritic-stripped word (when remove_diacritics)
**   other word   -> ASCII-lowercased word (unless case_sensitive)
** These must never change: they are what old indexes and new queries agree on.
*/
static int process_word(arabic_phonetic_fuzzy_trigram_tokenizer *pTok,
                        void *pCtx, const char *token, int token_len,
                        int token_start, int token_end,
                        int (*xToken)(void *, int, const char *, int, int, int)) {
    int rc = SQLITE_OK;

    if (is_arabic_word(token, token_len) && pTok->bRemoveDiacritics) {
        /* ---- Arabic word ---- */
        int clean_len;
        char *clean = remove_diacritic(token, token_len, &clean_len);
        if (clean && clean_len > 0) {
            rc = xToken(pCtx, 0, clean, clean_len, token_start, token_end);  /* PRIMARY */

            if (rc == SQLITE_OK && pTok->bGenerateTrigrams) {
                rc = emit_trigrams_arabic_compat(clean, clean_len, pCtx,
                                                 token_start, token_end, xToken);
            }

            /* Persian/Urdu variant + Arabic-Indic digit canonicalization,
            ** as a colocated synonym. Trigrams of the canonical form are
            ** emitted too so partial-word matches also bridge variants. */
            if (rc == SQLITE_OK && pTok->bGenerateNormalized) {
                int canon_len;
                char *canon = arabic_canonicalize(clean, clean_len, &canon_len);
                if (canon) {
                    rc = xToken(pCtx, FTS5_TOKEN_COLOCATED, canon, canon_len,
                                token_start, token_end);
                    if (rc == SQLITE_OK && pTok->bGenerateTrigrams) {
                        rc = emit_trigrams_arabic_compat(canon, canon_len, pCtx,
                                                         token_start, token_end,
                                                         xToken);
                    }
                    sqlite3_free(canon);
                }
            }
        }
        if (clean) sqlite3_free(clean);

        /* Transliteration — for every Arabic word. (0.0.x required the word
        ** to carry diacritics, which made undiacritized titles invisible to
        ** Latin queries.) */
        if (rc == SQLITE_OK && pTok->bTransliterate) {
            int translit_len;
            char *translit = transliterate_text(token, token_len, &translit_len);
            if (translit && translit_len > 0) {
                if (!pTok->bCaseSensitive) {
                    lowercase_ascii(translit, translit_len);
                }
                rc = xToken(pCtx, FTS5_TOKEN_COLOCATED, translit, translit_len,
                            token_start, token_end);

                if (rc == SQLITE_OK && pTok->bGenerateNormalized) {
                    rc = emit_normalized(translit, translit_len, pCtx,
                                         token_start, token_end, xToken);
                }
            }
            if (translit) sqlite3_free(translit);
        }
        return rc;
    }

    /* ---- Non-Arabic word ---- */
    char *lowered = sqlite3_malloc(token_len + 1);
    if (!lowered) return SQLITE_NOMEM;
    memcpy(lowered, token, token_len);
    lowered[token_len] = '\0';
    if (!pTok->bCaseSensitive) {
        lowercase_ascii(lowered, token_len);
    }

    rc = xToken(pCtx, 0, lowered, token_len, token_start, token_end);  /* PRIMARY */

    if (rc == SQLITE_OK && is_all_ascii(token, token_len)) {
        /* Latin/ASCII: the normalized form is the fuzzy token. Character
        ** n-grams are deliberately NOT emitted for ASCII words — they would
        ** make every 3-letter fragment of every word a match and destroy
        ** precision; normalization covers the observed error classes
        ** (doubled letters, vowel-length variants, apostrophes). */
        if (pTok->bGenerateNormalized) {
            rc = emit_normalized(lowered, token_len, pCtx,
                                 token_start, token_end, xToken);
        }
    } else if (rc == SQLITE_OK) {
        /* Other scripts (Bengali, Devanagari, Gujarati, CJK, ...): char
        ** n-grams give typo tolerance and substring matching. CJK also gets
        ** bigrams: most Chinese words are two characters. */
        if (pTok->bGenerateTrigrams) {
            int nmin = contains_cjk(token, token_len) ? 2 : 3;
            rc = emit_ngrams_unicode(lowered, token_len, nmin, 3, pCtx,
                                     token_start, token_end, xToken);
        }

        /* Latin-diacritic folding (é -> e, ü -> u, ...) */
        if (rc == SQLITE_OK && pTok->bTransliterate) {
            int translit_len;
            char *translit = transliterate_text(token, token_len, &translit_len);
            if (translit && translit_len > 0 &&
                (translit_len != token_len || memcmp(token, translit, token_len) != 0)) {
                if (!pTok->bCaseSensitive) {
                    lowercase_ascii(translit, translit_len);
                }
                rc = xToken(pCtx, FTS5_TOKEN_COLOCATED, translit, translit_len,
                            token_start, token_end);
                if (rc == SQLITE_OK && pTok->bGenerateNormalized) {
                    rc = emit_normalized(translit, translit_len, pCtx,
                                         token_start, token_end, xToken);
                }
            }
            if (translit) sqlite3_free(translit);
        }
    }

    sqlite3_free(lowered);
    return rc;
}

/*
** Main tokenization function
*/
static int arabic_phonetic_fuzzy_trigram_tokenize(
        Fts5Tokenizer *pTokenizer,
        void *pCtx,
        int flags,
        const char *pText, int nText,
        int (*xToken)(void *, int, const char *, int, int, int)
) {
    arabic_phonetic_fuzzy_trigram_tokenizer *pTok = (arabic_phonetic_fuzzy_trigram_tokenizer *) pTokenizer;
    int rc = SQLITE_OK;
    int pos = 0;
    int token_start = 0;

    (void) flags;  /* emission is symmetric for documents and queries */

    while (pos < nText && rc == SQLITE_OK) {
        int bytes_consumed;
        int codepoint = get_unicode_codepoint(pText + pos, nText - pos, &bytes_consumed);

        if (codepoint == -1) {
            pos++;
            continue;
        }

        if (is_word_separator(codepoint)) {
            if (pos > token_start) {
                rc = process_word(pTok, pCtx, pText + token_start, pos - token_start,
                                  token_start, pos, xToken);
            }

            /* Skip separators */
            while (pos < nText) {
                int next_bytes;
                int next_codepoint = get_unicode_codepoint(pText + pos, nText - pos, &next_bytes);
                if (next_codepoint == -1 || !is_word_separator(next_codepoint)) break;
                pos += next_bytes;
            }
            token_start = pos;
            continue;
        }

        pos += bytes_consumed;
    }

    /* Final token */
    if (pos > token_start && rc == SQLITE_OK) {
        rc = process_word(pTok, pCtx, pText + token_start, pos - token_start,
                          token_start, pos, xToken);
    }

    return rc;
}

/*
** Tokenizer module definition
*/
static fts5_tokenizer arabic_phonetic_fuzzy_trigram_tokenizer_module = {
        arabic_phonetic_fuzzy_trigram_create,
        arabic_phonetic_fuzzy_trigram_delete,
        arabic_phonetic_fuzzy_trigram_tokenize
};

/*
** SELECT arabic_phonetic_fuzzy_trigram_tokens(text [, config]) -> JSON
**
** Debug/QA introspection: returns the emitted token stream as a JSON array
** of [token, colocated] pairs, e.g.
**
**   SELECT arabic_phonetic_fuzzy_trigram_tokens('Subhaanallaahi');
**   -- [["subhaanallaahi",0],["subhanalahi",1]]
**
** colocated=0 is the primary token, colocated=1 an FTS5 synonym. The
** optional second argument is a tokenizer argument string; pass the exact
** args from a table's tokenize= clause (the leading tokenizer name is
** allowed and skipped) to reproduce that table's emission. Without it,
** defaults apply.
*/
typedef struct TokensDumpCtx {
    sqlite3_str *pStr;
    int nTok;
} TokensDumpCtx;

static int tokens_dump_callback(void *pCtx, int tflags,
                                const char *pToken, int nToken,
                                int iStart, int iEnd) {
    TokensDumpCtx *p = (TokensDumpCtx *) pCtx;
    (void) iStart;
    (void) iEnd;

    sqlite3_str_appendall(p->pStr, p->nTok ? ",[\"" : "[\"");
    for (int i = 0; i < nToken; i++) {
        unsigned char c = (unsigned char) pToken[i];
        if (c == '"' || c == '\\') {
            sqlite3_str_appendchar(p->pStr, 1, '\\');
            sqlite3_str_appendchar(p->pStr, 1, (char) c);
        } else if (c < 0x20) {
            sqlite3_str_appendf(p->pStr, "\\u%04x", (int) c);
        } else {
            sqlite3_str_appendchar(p->pStr, 1, (char) c);
        }
    }
    sqlite3_str_appendf(p->pStr, "\",%d]",
                        (tflags & FTS5_TOKEN_COLOCATED) ? 1 : 0);
    p->nTok++;
    return SQLITE_OK;
}

static void arabic_phonetic_fuzzy_trigram_tokens_func(
        sqlite3_context *pCtx, int nArg, sqlite3_value **apArg) {
    const char *zText = (const char *) sqlite3_value_text(apArg[0]);
    int nText = sqlite3_value_bytes(apArg[0]);
    const char *azArg[32];
    int nTokArg = 0;
    char *zConfig = 0;
    const char **azUse = azArg;
    Fts5Tokenizer *pTok = 0;
    TokensDumpCtx ctx;
    char *zOut;

    if (!zText) {
        sqlite3_result_null(pCtx);
        return;
    }

    if (nArg == 2 && sqlite3_value_text(apArg[1])) {
        zConfig = sqlite3_mprintf("%s", sqlite3_value_text(apArg[1]));
        if (!zConfig) {
            sqlite3_result_error_nomem(pCtx);
            return;
        }
        char *z = zConfig;
        while (*z && nTokArg < 32) {
            while (*z == ' ') *z++ = '\0';
            if (*z) {
                azArg[nTokArg++] = z;
                while (*z && *z != ' ') z++;
            }
        }
        /* Allow passing the whole tokenize= string: skip the leading name */
        if (nTokArg > 0 &&
            strcmp(azArg[0], "arabic_phonetic_fuzzy_trigram") == 0) {
            azUse = azArg + 1;
            nTokArg--;
        }
    }

    if (arabic_phonetic_fuzzy_trigram_create(0, azUse, nTokArg, &pTok) != SQLITE_OK) {
        sqlite3_free(zConfig);
        sqlite3_result_error_nomem(pCtx);
        return;
    }

    ctx.pStr = sqlite3_str_new(sqlite3_context_db_handle(pCtx));
    ctx.nTok = 0;
    sqlite3_str_appendchar(ctx.pStr, 1, '[');
    arabic_phonetic_fuzzy_trigram_tokenize(pTok, &ctx, 0, zText, nText,
                                           tokens_dump_callback);
    sqlite3_str_appendchar(ctx.pStr, 1, ']');

    zOut = sqlite3_str_finish(ctx.pStr);
    if (zOut) {
        sqlite3_result_text(pCtx, zOut, -1, sqlite3_free);
    } else {
        sqlite3_result_error_nomem(pCtx);
    }

    arabic_phonetic_fuzzy_trigram_delete(pTok);
    sqlite3_free(zConfig);
}

/*
** SELECT arabic_phonetic_fuzzy_trigram_normalize(word) -> TEXT
**
** The universal search_vocab lookup-key transform, exposed to SQL so
** consumers (the mobile app's typo-correction fallback, backend vocabulary
** generation, QA tooling) never reimplement the rules — the C implementation
** is the single source of truth. Routed by script:
**
**   ASCII/Latin   -> normalize_latin_ascii (lowercase, drop '/-,
**                    ee->i oo->u ou->u q->k, collapse doubles)
**   Arabic script -> remove_diacritic + variant/digit canonicalization —
**                    the same normalized space as the FTS primary tokens
**   other scripts -> unchanged (Indic vocabulary keys are the raw words)
*/
static void arabic_phonetic_fuzzy_trigram_normalize_func(
        sqlite3_context *pCtx, int nArg, sqlite3_value **apArg) {
    const char *zIn = (const char *) sqlite3_value_text(apArg[0]);
    (void) nArg;

    if (!zIn) {
        sqlite3_result_null(pCtx);
        return;
    }
    int nIn = sqlite3_value_bytes(apArg[0]);

    if (is_all_ascii(zIn, nIn)) {
        char *zOut = sqlite3_malloc(nIn + 1);
        if (!zOut) {
            sqlite3_result_error_nomem(pCtx);
            return;
        }
        int nOut = normalize_latin_ascii(zIn, nIn, zOut);
        sqlite3_result_text(pCtx, zOut, nOut, sqlite3_free);
        return;
    }

    if (is_arabic_word(zIn, nIn)) {
        int nClean;
        char *zClean = remove_diacritic(zIn, nIn, &nClean);
        if (!zClean) {
            sqlite3_result_error_nomem(pCtx);
            return;
        }
        int nCanon;
        char *zCanon = arabic_canonicalize(zClean, nClean, &nCanon);
        if (zCanon) {
            sqlite3_free(zClean);
            sqlite3_result_text(pCtx, zCanon, nCanon, sqlite3_free);
        } else {
            sqlite3_result_text(pCtx, zClean, nClean, sqlite3_free);
        }
        return;
    }

    /* Other scripts (Bengali, Devanagari, Gujarati, ...): identity */
    sqlite3_result_text(pCtx, zIn, nIn, SQLITE_TRANSIENT);
}

/*
** SELECT arabic_phonetic_fuzzy_trigram_editdist(a, b [, maxd]) -> INTEGER
**
** Levenshtein edit distance between two strings, for query-time typo
** correction against the `search_vocab` table that DB generation builds:
**
**   SELECT display FROM search_vocab
**   WHERE arabic_phonetic_fuzzy_trigram_editdist(word, :q, 2) <= 2
**   ORDER BY arabic_phonetic_fuzzy_trigram_editdist(word, :q, 2), rank DESC
**   LIMIT 3;
**
** (:q is the user's word passed through normalize_latin — the vocab stores
** normalized forms.)
**
** The optional third argument is a distance cap: rows abort as soon as the
** running row-minimum exceeds it, returning 999. Most vocabulary words
** diverge from a query within a few characters, so the capped form scans
** ~3x faster (verified equivalent to the exact form for all results <= maxd
** over 3M random pairs). Without maxd the exact distance is returned.
**
** Returns 999 when either string exceeds 63 bytes or the length difference
** exceeds 8 (never useful correction candidates; keeps DP buffers on the
** stack). NULL in, NULL out. Candidates are real corpus words, so unlike
** hash-bucket matching a suggestion can never be an unrelated word.
*/
#define EDITDIST_MAX_WORD 64

static void arabic_phonetic_fuzzy_trigram_editdist_func(
        sqlite3_context *pCtx, int nArg, sqlite3_value **apArg) {
    const unsigned char *a = sqlite3_value_text(apArg[0]);
    const unsigned char *b = sqlite3_value_text(apArg[1]);

    if (!a || !b) {
        sqlite3_result_null(pCtx);
        return;
    }

    int na_bytes = sqlite3_value_bytes(apArg[0]);
    int nb_bytes = sqlite3_value_bytes(apArg[1]);
    int maxd = -1;  /* -1: exact mode (no cap) */
    if (nArg == 3) {
        maxd = sqlite3_value_int(apArg[2]);
        if (maxd < 0) maxd = 0;
    }

    /* Decode to codepoints: the DP must count CHARACTERS, not bytes.
    ** Byte-level distance is an encoding artifact for multibyte scripts —
    ** most Arabic letters share a lead byte, so some one-letter typos cost
    ** 1 byte and others 2, and thresholds stop meaning "number of typos".
    ** For pure-ASCII input this is byte-identical to the old behavior. */
    int ca[EDITDIST_MAX_WORD], cb[EDITDIST_MAX_WORD];
    int na = 0, nb = 0, pos = 0;
    while (pos < na_bytes) {
        int l;
        int cp = get_unicode_codepoint((const char *) a + pos, na_bytes - pos, &l);
        if (na >= EDITDIST_MAX_WORD) { sqlite3_result_int(pCtx, 999); return; }
        ca[na++] = (cp < 0) ? (unsigned char) a[pos] : cp;
        pos += (l > 0) ? l : 1;
    }
    pos = 0;
    while (pos < nb_bytes) {
        int l;
        int cp = get_unicode_codepoint((const char *) b + pos, nb_bytes - pos, &l);
        if (nb >= EDITDIST_MAX_WORD) { sqlite3_result_int(pCtx, 999); return; }
        cb[nb++] = (cp < 0) ? (unsigned char) b[pos] : cp;
        pos += (l > 0) ? l : 1;
    }

    if (na - nb > 8 || nb - na > 8 ||
        (maxd >= 0 && (na - nb > maxd || nb - na > maxd))) {
        sqlite3_result_int(pCtx, 999);
        return;
    }

    int prev[EDITDIST_MAX_WORD + 1];
    int cur[EDITDIST_MAX_WORD + 1];
    for (int j = 0; j <= nb; j++) prev[j] = j;
    for (int i = 1; i <= na; i++) {
        cur[0] = i;
        int rowmin = i;
        for (int j = 1; j <= nb; j++) {
            int m = prev[j] + 1;                          /* deletion */
            if (cur[j - 1] + 1 < m) m = cur[j - 1] + 1;   /* insertion */
            int cost = (ca[i - 1] != cb[j - 1]);
            if (prev[j - 1] + cost < m) m = prev[j - 1] + cost;  /* subst */
            cur[j] = m;
            if (m < rowmin) rowmin = m;
        }
        if (maxd >= 0 && rowmin > maxd) {
            /* No cell can decrease in later rows: distance > maxd, abort. */
            sqlite3_result_int(pCtx, 999);
            return;
        }
        memcpy(prev, cur, sizeof(int) * (nb + 1));
    }
    if (maxd >= 0 && prev[nb] > maxd) {
        sqlite3_result_int(pCtx, 999);
        return;
    }
    sqlite3_result_int(pCtx, prev[nb]);
}

/*
** SELECT arabic_phonetic_fuzzy_trigram_version() -> '0.1.0'
**
** Lets any consumer (backend DB generation, the mobile app, QA tooling)
** verify at runtime which tokenizer build is loaded. Builds prior to 0.1.0
** do not provide this function ("no such function" identifies them).
*/
static void arabic_phonetic_fuzzy_trigram_version_func(
        sqlite3_context *pCtx, int nArg, sqlite3_value **apArg) {
    (void) nArg;
    (void) apArg;
    sqlite3_result_text(pCtx, ARABIC_PHONETIC_FUZZY_TRIGRAM_VERSION, -1, SQLITE_STATIC);
}

/*
** Register the tokenizer module
*/
static int register_arabic_phonetic_fuzzy_trigram_tokenizer(sqlite3 *db) {
    int rc;
    fts5_api *pApi = 0;
    sqlite3_stmt *pStmt = 0;

    /* Get FTS5 API */
    rc = sqlite3_prepare(db, "SELECT fts5(?1)", -1, &pStmt, 0);
    if (rc != SQLITE_OK) return rc;

    sqlite3_bind_pointer(pStmt, 1, (void *) &pApi, "fts5_api_ptr", NULL);
    rc = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);

    if (rc != SQLITE_ROW || pApi == NULL) {
        return SQLITE_ERROR;
    }

    /* Register tokenizer */
    rc = pApi->xCreateTokenizer(pApi, "arabic_phonetic_fuzzy_trigram",
                                (void *) pApi,
                                &arabic_phonetic_fuzzy_trigram_tokenizer_module,
                                NULL);
    if (rc != SQLITE_OK) return rc;

    /* Register the version function */
    rc = sqlite3_create_function(db, "arabic_phonetic_fuzzy_trigram_version", 0,
                                 SQLITE_UTF8 | SQLITE_DETERMINISTIC
#ifdef SQLITE_INNOCUOUS
            | SQLITE_INNOCUOUS
#endif
            , NULL,
                                 arabic_phonetic_fuzzy_trigram_version_func,
                                 NULL, NULL);
    if (rc != SQLITE_OK) return rc;

    /* Register the token-stream introspection function (1- and 2-arg) */
    for (int nArg = 1; nArg <= 2 && rc == SQLITE_OK; nArg++) {
        rc = sqlite3_create_function(db, "arabic_phonetic_fuzzy_trigram_tokens",
                                     nArg,
                                     SQLITE_UTF8 | SQLITE_DETERMINISTIC
#ifdef SQLITE_INNOCUOUS
                | SQLITE_INNOCUOUS
#endif
                , NULL,
                                     arabic_phonetic_fuzzy_trigram_tokens_func,
                                     NULL, NULL);
    }
    if (rc != SQLITE_OK) return rc;

    /* Register the normalization function (single source of truth for the
    ** search_vocab lookup-key format) */
    rc = sqlite3_create_function(db, "arabic_phonetic_fuzzy_trigram_normalize", 1,
                                 SQLITE_UTF8 | SQLITE_DETERMINISTIC
#ifdef SQLITE_INNOCUOUS
            | SQLITE_INNOCUOUS
#endif
            , NULL,
                                 arabic_phonetic_fuzzy_trigram_normalize_func,
                                 NULL, NULL);
    if (rc != SQLITE_OK) return rc;

    /* Register the edit-distance function (query-time typo correction):
    ** 2-arg exact, 3-arg with distance cap + early abort. */
    for (int nArg = 2; nArg <= 3 && rc == SQLITE_OK; nArg++) {
        rc = sqlite3_create_function(db, "arabic_phonetic_fuzzy_trigram_editdist",
                                     nArg,
                                     SQLITE_UTF8 | SQLITE_DETERMINISTIC
#ifdef SQLITE_INNOCUOUS
                | SQLITE_INNOCUOUS
#endif
                , NULL,
                                     arabic_phonetic_fuzzy_trigram_editdist_func,
                                     NULL, NULL);
    }

    return rc;
}

/*
** Extension entry point
*/
#ifdef _WIN32
__declspec(dllexport)
#endif

int sqlite3_arabic_phonetic_fuzzy_trigram_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi
) {
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void) pzErrMsg;

    rc = register_arabic_phonetic_fuzzy_trigram_tokenizer(db);
    return rc;
}

/* Alternative entry point for backwards compatibility */
#ifdef _WIN32
__declspec(dllexport)
#endif

int sqlite3_extension_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi
) {
    return sqlite3_arabic_phonetic_fuzzy_trigram_init(db, pzErrMsg, pApi);
}
