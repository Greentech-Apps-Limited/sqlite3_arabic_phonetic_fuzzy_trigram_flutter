/**
 * Header file for the arabic_phonetic_fuzzy_trigram FTS5 tokenizer extension for SQLite.
 *
 * This extension provides an FTS5 tokenizer named 'fuzzy_trigram' which is
 * specifically designed for robust fuzzy searching of Arabic and Latin text.
 *
 * When registered with FTS5, it automatically normalizes text by:
 * - Removing Arabic diacritics and punctuation.
 * - basic phonetic search.
 * - fuzzy search
 * - Collapsing repeated vowels in Latin script.
 *
 * The normalized text is then indexed as a stream of 3-character trigrams,
 * providing tolerance for typos and spelling variations.
 */
#ifndef ARABIC_PHONETIC_FUZZY_TRIGRAM_H
#define ARABIC_PHONETIC_FUZZY_TRIGRAM_H

#include "sqlite3ext.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The initialization function for the arabic_phonetic_trigram SQLite extension.
 *
 * This function registers the 'arabic_phonetic_fuzzy_trigram' FTS5 tokenizer with the given
 * database connection. It should be called when loading the extension.
 *
 * @param db The SQLite database handle.
 * @param pzErrMsg A pointer to a string where an error message can be stored.
 * @param pApi A pointer to the SQLite API routines structure.
 * @return SQLITE_OK on success, or an error code on failure.
 */
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_arabic_phonetic_fuzzy_trigram_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ARABIC_PHONETIC_FUZZY_TRIGRAM_H