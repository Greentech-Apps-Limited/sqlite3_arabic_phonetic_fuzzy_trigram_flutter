/*
** SQLite FTS5 Arabic Tokenizer with Transliteration and Better Trigram Support
**
** Based on GreentechApps/sqlite3-arabic-tokenizer approach
** Combined with streetwriters/sqlite-better-trigram and nalgeon/sqlean translit
*/

#include "sqlite3-arabic-phonetic-fuzzy-trigram.h"
#include "sqlite3ext.h"

SQLITE_EXTENSION_INIT1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "common.c"


// Ooriginally from the spellfix SQLite exension, Public Domain
// https://www.sqlite.org/src/file/ext/misc/spellfix.c
// Modified by Anton Zhiyanov, https://github.com/nalgeon/sqlean/, MIT License

extern const unsigned char midClass[];
extern const unsigned char initClass[];
extern const unsigned char className[];

/*
** Generate a "phonetic hash" from a string of ASCII characters
** in zIn[0..nIn-1].
**
**   * Map characters by character class as defined above.
**   * Omit double-letters
**   * Omit vowels beside R and L
**   * Omit T when followed by CH
**   * Omit W when followed by R
**   * Omit D when followed by J or G
**   * Omit K in KN or G in GN at the beginning of a word
**
** Space to hold the result is obtained from sqlite3_malloc()
**
** Return NULL if memory allocation fails.
*/
unsigned char *phonetic_hash(const unsigned char *zIn, int nIn) {
    unsigned char *zOut = sqlite3_malloc(nIn + 1);
    int i;
    int nOut = 0;
    char cPrev = 0x77;
    char cPrevX = 0x77;
    const unsigned char *aClass = initClass;

    if (zOut == 0)
        return 0;
    if (nIn > 2) {
        switch (zIn[0]) {
            case 'g':
            case 'k': {
                if (zIn[1] == 'n') {
                    zIn++;
                    nIn--;
                }
                break;
            }
        }
    }
    for (i = 0; i < nIn; i++) {
        unsigned char c = zIn[i];
        if (i + 1 < nIn) {
            if (c == 'w' && zIn[i + 1] == 'r')
                continue;
            if (c == 'd' && (zIn[i + 1] == 'j' || zIn[i + 1] == 'g'))
                continue;
            if (i + 2 < nIn) {
                if (c == 't' && zIn[i + 1] == 'c' && zIn[i + 2] == 'h')
                    continue;
            }
        }
        c = aClass[c & 0x7f];
        if (c == CCLASS_SPACE)
            continue;
        if (c == CCLASS_OTHER && cPrev != CCLASS_DIGIT)
            continue;
        aClass = midClass;
        if (c == CCLASS_VOWEL && (cPrevX == CCLASS_R || cPrevX == CCLASS_L)) {
            continue; /* No vowels beside L or R */
        }
        if ((c == CCLASS_R || c == CCLASS_L) && cPrevX == CCLASS_VOWEL) {
            nOut--; /* No vowels beside L or R */
        }
        cPrev = c;
        if (c == CCLASS_SILENT)
            continue;
        cPrevX = c;
        c = className[c];
        assert(nOut >= 0);
        if (nOut == 0 || c != zOut[nOut - 1])
            zOut[nOut++] = c;
    }
    zOut[nOut] = 0;
    return zOut;
}

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
    int bGenerateTrigrams;    /* Generate trigram tokens */
    int bTransliterate;       /* Generate transliterated tokens */
    int bGeneratePhonetic;    /* Generate phonetic hash tokens */
    int bCaseSensitive;       /* Case sensitive tokenization */
};

typedef unsigned char utf8_t;

#define isunicode(c) (((c)&0xc0)==0xc0)

static int arabic_unicode[67] = {
        1548, // arabic comma
        1552,
        1553,
        1554,
        1555,
        1556,
        1557,
        1558,
        1559,
        1560,
        1561,
        1562,
        1750,
        1751,
        1752,
        1753,
        1754,
        1755,
        1756,
        1757,
        1758,
        1759,
        1760,
        1761,
        1762,
        1763,
        1764,
        1765,
        1766,
        1767,
        1768,
        1769,
        1770,
        1771,
        1772,
        1773,
        1600,
        1611,
        1612,
        1613,
        1614,
        1615,
        1616,
        1617,
        1618,
        1619,
        1620,
        1621,
        1622,
        1623,
        1624,
        1625,
        1626,
        1627,
        1628,
        1629,
        1630,
        1631,
        1648, // 59

        1571, 1573, 1570, 1649, 1671, // 64 alif replace
        1610,
        1569,
        1607, // 67
};

char *aliff = "ا";
char *r1 = "ى";
char *r2 = "ئ";
char *r3 = "ة";

int unicode_diacritic(int code) {

    int found = -1;
    for (int i = 0; i < 67; i++) {
        if (arabic_unicode[i] == code) {
            found = i;
            break;
        }
    }

    return found;
}

int utf8_decode(const char *str, int *i) {
    const utf8_t *s = (const utf8_t *) str; // Use unsigned chars
    int u = *s, l = 1;
    if (isunicode(u)) {
        int a = (u & 0x20) ? ((u & 0x10) ? ((u & 0x08) ? ((u & 0x04) ? 6 : 5) : 4) : 3) : 2;
        if (a < 6 || !(u & 0x02)) {
            int b, p = 0;
            u = ((u << (a + 1)) & 0xff) >> (a + 1);
            for (b = 1; b < a; ++b)
                u = (u << 6) | (s[l++] & 0x3f);
        }
    }
    if (i) *i += l;
    return u;
}

static int has_diacritics(const char *text, int text_len) {
    int i = 0;
    while (i < text_len) {
        if (isunicode(text[i])) {
            int l = 0;
            int z = utf8_decode(&text[i], &l);
            if (unicode_diacritic(z) != -1 && unicode_diacritic(z) < 59) {
                return 1;
            }
            i += l;
        } else {
            i++;
        }
    }
    return 0;
}

/*
** Improved UTF-8 codepoint extraction
** More robust error handling
*/
static int get_unicode_codepoint(const char *text, int *bytes_consumed) {
    const unsigned char *utf8 = (const unsigned char *) text;
    int codepoint = 0;

    /* Single byte ASCII */
    if ((utf8[0] & 0x80) == 0) {
        codepoint = utf8[0];
        *bytes_consumed = 1;
    }
        /* Two byte sequence (110xxxxx 10xxxxxx) */
    else if ((utf8[0] & 0xE0) == 0xC0) {
        if ((utf8[1] & 0xC0) != 0x80) {
            *bytes_consumed = 1;
            return -1; /* Invalid sequence */
        }
        codepoint = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
        *bytes_consumed = 2;
    }
        /* Three byte sequence (1110xxxx 10xxxxxx 10xxxxxx) */
    else if ((utf8[0] & 0xF0) == 0xE0) {
        if ((utf8[1] & 0xC0) != 0x80 || (utf8[2] & 0xC0) != 0x80) {
            *bytes_consumed = 1;
            return -1; /* Invalid sequence */
        }
        codepoint = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
        *bytes_consumed = 3;
    }
        /* Four byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx) */
    else if ((utf8[0] & 0xF8) == 0xF0) {
        if ((utf8[1] & 0xC0) != 0x80 || (utf8[2] & 0xC0) != 0x80 || (utf8[3] & 0xC0) != 0x80) {
            *bytes_consumed = 1;
            return -1; /* Invalid sequence */
        }
        codepoint = ((utf8[0] & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) |
                    ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
        *bytes_consumed = 4;
    } else {
        *bytes_consumed = 1;
        return -1; /* Invalid UTF-8 start byte */
    }

    /* Check for overlong encoding or invalid codepoints */
    if ((*bytes_consumed == 2 && codepoint < 0x80) ||
        (*bytes_consumed == 3 && codepoint < 0x800) ||
        (*bytes_consumed == 4 && codepoint < 0x10000) ||
        codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
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
** Check if a word is Arabic by checking the first character
** Returns 1 if the first character is Arabic, 0 otherwise
*/
static int is_arabic_word(const char *text, int text_len) {
    if (!text || text_len <= 0) {
        return 0;
    }

    int bytes_consumed;
    int first_codepoint = get_unicode_codepoint(text, &bytes_consumed);

    if (first_codepoint == -1) {
        return 0;
    }

    return is_arabic_char(first_codepoint);
}

char *remove_diacritic(const char *text, int input_len, int *output_len) {
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
        if (!isunicode(text[i])) {
            replaced[j++] = text[i];
            i++;
        } else {
            int l = 0;
            int z = utf8_decode(&text[i], &l);

            // Make sure we don't go beyond input_len
            if (i + l > input_len) {
                break;
            }

            i += l;
            int index = unicode_diacritic(z);

            if (index == -1) {
                // Copy the original UTF-8 bytes
                for (int k = 0; k < l && j < input_len + 4; k++) {
                    replaced[j++] = text[i - l + k];
                }
            } else if (index >= 59 && index <= 63) {
                replaced[j++] = aliff[0];
                replaced[j++] = aliff[1];
            } else if (index >= 64 && index <= 66) {
                if (index == 64) {
                    replaced[j++] = r1[0];
                    replaced[j++] = r1[1];
                } else if (index == 65) {
                    replaced[j++] = r2[0];
                    replaced[j++] = r2[1];
                } else if (index == 66) {
                    replaced[j++] = r3[0];
                    replaced[j++] = r3[1];
                }
            }
            // If index is something else (diacritic), skip it (don't add to output)
        }
    }

    replaced[j] = '\0';
    *output_len = j;
    return replaced;
}

static struct {
    int unicode;
    const char *ascii;
} arabic_translit_table[] = {
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
        {0,      NULL}
};


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
        int codepoint = get_unicode_codepoint(input + input_pos, &bytes_consumed);

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
            int i;
            for (i = 0; arabic_translit_table[i].ascii != NULL; i++) {
                if (arabic_translit_table[i].unicode == prev_codepoint) {
                    const char *translit = arabic_translit_table[i].ascii;
                    int len = strlen(translit);
                    if (output_pos + len < input_len * 4) {
                        memcpy(output + output_pos, translit, len);
                        output_pos += len;
                    }
                    break;
                }
            }
            input_pos += bytes_consumed;
            continue;
        }

        /* Look up transliteration */
        const char *translit = NULL;
        int i;
        for (i = 0; arabic_translit_table[i].ascii != NULL; i++) {
            if (arabic_translit_table[i].unicode == codepoint) {
                translit = arabic_translit_table[i].ascii;
                break;
            }
        }

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
        int codepoint = get_unicode_codepoint(input + input_pos, &bytes_consumed);

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

        /* Look up in Latin diacritics table */
        const char *translit = NULL;
        int i;

        /* Latin diacritics */
        static struct {
            int unicode;
            const char *ascii;
        } latin_table[] = {
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
                {0x1E62, "S"},
                {0x1E63, "s"},
                {0,      NULL}
        };

        for (i = 0; latin_table[i].ascii != NULL; i++) {
            if (latin_table[i].unicode == codepoint) {
                translit = latin_table[i].ascii;
                break;
            }
        }

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
    if (codepoint >= 0x06D4 && codepoint <= 0x06D4) return 1; /* Arabic full stop */

    /* Arabic-specific separators */
    if (codepoint == 0x0020) return 1; /* Regular space */
    if (codepoint == 0x200C) return 1; /* Zero-width non-joiner */
    if (codepoint == 0x200D) return 1; /* Zero-width joiner */

    /* General punctuation */
    if (codepoint >= 0x2010 && codepoint <= 0x2027) return 1;
    if (codepoint >= 0x2030 && codepoint <= 0x205E) return 1;

    return 0;
}

/*
** Generate trigrams for a word
*/
static int emit_trigrams(const char *word, int word_len,
                         void *pCtx, int start, int end,
                         int (*xToken)(void *, int, const char *, int, int, int)) {
    if (word_len < 3) {
        /* For words shorter than 3 characters, emit as-is */
        return xToken(pCtx, 1, word, word_len, start, end);
    }

    /* Generate overlapping trigrams */
    int i;
    for (i = 0; i <= word_len - 3; i++) {
        int rc = xToken(pCtx, 1, word + i, 3, start, end);
        if (rc != SQLITE_OK) return rc;
    }

    return SQLITE_OK;
}

static int emit_trigrams_arabic(const char *word, int word_len,
                                void *pCtx, int start, int end,
                                int (*xToken)(void *, int, const char *, int, int, int)) {
    // Count characters and store byte positions
    int char_positions[256];  // byte offset for each character
    int char_count = 0;
    int i = 0;

    while (i < word_len && char_count < 255) {
        char_positions[char_count++] = i;
        int bytes;
        get_unicode_codepoint(word + i, &bytes);
        i += bytes;
    }
    char_positions[char_count] = word_len;  // end marker

    if (char_count < 3) {
        return xToken(pCtx, 1, word, word_len, start, end);
    }

    // Emit 3-character trigrams
    for (i = 0; i <= char_count - 3; i++) {
        int trigram_start = char_positions[i];
        int trigram_end = char_positions[i + 3];
        int trigram_len = trigram_end - trigram_start;

        int rc = xToken(pCtx, 1, word + trigram_start, trigram_len, start, end);
        if (rc != SQLITE_OK) return rc;
    }

    return SQLITE_OK;
}

/*
** Apply phonetic patterns (salah -> salat, etc.)
*/
static int generate_phonetic_hash(const char *word, int word_len,
                                  void *pCtx, int start, int end,
                                  int (*xToken)(void *, int, const char *, int, int, int), int flags, char *text) {
    int rc = SQLITE_OK;

    unsigned char *generated_hash = phonetic_hash((const unsigned char *) word, word_len);

    if (generated_hash == NULL) {
        return SQLITE_IOERR_NOMEM; // Error: memory allocation failed
    }

    // Calculate the length of the generated hash
    int hash_length = strlen((const char *) generated_hash);

    // Call the xToken function with the generated hash
    int result = xToken(pCtx, flags, (const char *) generated_hash, hash_length, start, end);
    // printf("generated hash '%s' '%.*s' %s\n\n", generated_hash, word_len, word, text);

    // Clean up allocated memory
    sqlite3_free(generated_hash);

    return result;
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
    pNew->bGeneratePhonetic = 1;
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
        } else if (strcmp(azArg[i], "generate_phonetic") == 0 && i + 1 < nArg) {
            pNew->bGeneratePhonetic = atoi(azArg[i + 1]);
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

    (void) flags;

    while (pos < nText && rc == SQLITE_OK) {
        int bytes_consumed;
        int codepoint = get_unicode_codepoint(pText + pos, &bytes_consumed);

        if (codepoint == -1) {
            pos++;
            continue;
        }

        if (is_word_separator(codepoint)) {
            /* End of token */
            if (pos > token_start) {
                const char *token = pText + token_start;
                int token_len = pos - token_start;

                if (is_arabic_word(token, token_len) && pTok->bRemoveDiacritics) {
                    int clean_len;
                    char *clean = remove_diacritic(token, token_len, &clean_len);
                    if (clean && clean_len > 0) {
                        //  printf("PRIMARY Arabic token: '%.*s' hex: ", clean_len, clean);
                        //  for(int k = 0; k < clean_len; k++) printf("%02x ", (unsigned char)clean[k]);
                        //   printf("\n");
                        rc = xToken(pCtx, 0, clean, clean_len, token_start, pos);  // PRIMARY
//                        if (rc == SQLITE_OK) {
//                            rc = generate_phonetic_hash(clean, clean_len, pCtx, token_start, pos, xToken, 1, "remove_diacritic");
//                        }
                        if (rc == SQLITE_OK && pTok->bGenerateTrigrams) {
                            if (is_arabic_word(clean, clean_len)) {
                                rc = emit_trigrams_arabic(clean, clean_len, pCtx, token_start, pos, xToken);
                            } else {
                                rc = emit_trigrams(clean, clean_len, pCtx, token_start, pos, xToken);
                            }
                        }
                    }
                    if (clean) sqlite3_free(clean);
                } else {
                    // Non-Arabic: check if we should generate phonetic hash
                    if (pTok->bGeneratePhonetic) {
                        rc = generate_phonetic_hash(token, token_len, pCtx, token_start, pos, xToken, 0, "default");
                    } else {
                        // If phonetic is disabled, we MUST emit the raw token as primary,
                        // otherwise this word is effectively lost to the index.
                        rc = xToken(pCtx, 0, token, token_len, token_start, pos);
                    }
                }



                //  printf("word '%.*s'\n\n", token_len, token);
                /* Transliterate if enabled */
                if (rc == SQLITE_OK && pTok->bTransliterate &&
                    (!is_arabic_word(token, token_len) || has_diacritics(token, token_len))) {
                    int translit_len;
                    char *translit = transliterate_text(token, token_len, &translit_len);
                    if (translit && translit_len > 0 &&
                        (translit_len != token_len || memcmp(token, translit, token_len) != 0)) {
                        rc = xToken(pCtx, 1, translit, translit_len, token_start, pos);
                        //  printf("translit %s\n\n", translit);
                        /* Apply phonetic patterns to transliterated text */
                        if (rc == SQLITE_OK && pTok->bGeneratePhonetic) {
                            rc = generate_phonetic_hash(translit, translit_len, pCtx, token_start, pos, xToken,1,
                                                        "translit");
                        }

                        /* Generate trigrams for transliterated text */
                        if (rc == SQLITE_OK && pTok->bGenerateTrigrams) {
                            rc = emit_trigrams(translit, translit_len, pCtx, token_start, pos, xToken);
                        }
                    }
                    if (translit) sqlite3_free(translit);
                }
            }

            /* Skip whitespace */
            while (pos < nText) {
                int next_bytes;
                int next_codepoint = get_unicode_codepoint(pText + pos, &next_bytes);
                if (next_codepoint == -1 || !is_word_separator(next_codepoint)) break;
                pos += next_bytes;
            }
            token_start = pos;
            continue;
        }

        pos += bytes_consumed;
    }

    /* Handle final token */
    if (pos > token_start && rc == SQLITE_OK) {
        const char *token = pText + token_start;
        int token_len = pos - token_start;

        /* Same processing as above for final token */
        if (is_arabic_word(token, token_len) && pTok->bRemoveDiacritics) {
            int clean_len;
            char *clean = remove_diacritic(token, token_len, &clean_len);
            if (clean && clean_len > 0) {
                // printf("PRIMARY Arabic token: '%.*s' hex: ", clean_len, clean);
                //   for(int k = 0; k < clean_len; k++) printf("%02x ", (unsigned char)clean[k]);
                //    printf("\n");
                rc = xToken(pCtx, 0, clean, clean_len, token_start, pos);  // PRIMARY
//                if (rc == SQLITE_OK) {
//                    rc = generate_phonetic_hash(clean, clean_len, pCtx, token_start, pos, xToken, 1, "remove_diacritic");
//                }
                if (rc == SQLITE_OK && pTok->bGenerateTrigrams) {
                    if (is_arabic_word(clean, clean_len)) {
                        rc = emit_trigrams_arabic(clean, clean_len, pCtx, token_start, pos, xToken);
                    } else {
                        rc = emit_trigrams(clean, clean_len, pCtx, token_start, pos, xToken);
                    }
                }
            }
            if (clean) sqlite3_free(clean);
        } else {
            // Non-Arabic: check if we should generate phonetic hash
            if (pTok->bGeneratePhonetic) {
                rc = generate_phonetic_hash(token, token_len, pCtx, token_start, pos, xToken, 0, "default");
            } else {
                // Fallback to raw token
                rc = xToken(pCtx, 0, token, token_len, token_start, pos);
            }
        }

        //  printf("word '%.*s'\n\n", token_len, token);
        if (rc == SQLITE_OK && pTok->bTransliterate &&
            (!is_arabic_word(token, token_len) || has_diacritics(token, token_len))) {
            int translit_len;
            char *translit = transliterate_text(token, token_len, &translit_len);
            if (translit && translit_len > 0 &&
                (translit_len != token_len || memcmp(token, translit, token_len) != 0)) {
                rc = xToken(pCtx, 1, translit, translit_len, token_start, pos);
                // printf("translit %s\n\n", translit);
                if (rc == SQLITE_OK && pTok->bGeneratePhonetic) {
                    rc = generate_phonetic_hash(translit, translit_len, pCtx, token_start, pos, xToken,1, "translit");
                }
                if (rc == SQLITE_OK && pTok->bGenerateTrigrams) {
                    rc = emit_trigrams(translit, translit_len, pCtx, token_start, pos, xToken);
                }
            }
            if (translit) sqlite3_free(translit);
        }
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