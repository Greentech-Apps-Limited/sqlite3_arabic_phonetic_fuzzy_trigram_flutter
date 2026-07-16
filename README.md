# sqlite3_arabic_phonetic_fuzzy_trigram_flutter

To
use [sqlite3-arabic-phonetic-fuzzy-trigram](https://github.com/Greentech-Apps-Limited/sqlite3-arabic-phonetic-fuzzy-trigram)
in Flutter.

# sqlite3-arabic-phonetic-fuzzy-trigram

A custom SQLite FTS5 tokenizer for multi-script search across Arabic,
Persian/Urdu/Pashto, Latin transliteration, Indic (Bengali, Devanagari,
Gujarati) and CJK text.

Diacritic-insensitive Arabic matching with character normalization,
cross-script search (Latin queries match Arabic text through indexed
transliterations), normalization-based fuzzy matching for spelling variants
(`subhanallah` ≡ `Subhaanallaahi` ≡ `du'a`/`dua`), character n-gram matching
for scripts without word separators, and built-in SQL helpers for typo
correction and debugging (`…_editdist`, `…_normalize`, `…_tokens`,
`…_version`).

## Features

### Arabic Text Support
- **Diacritic-insensitive search**: Matches Arabic text with or without diacritics (tashkeel)
    - `الحمد` matches `ٱلْحَمْدُ`
- **Character normalization**: Normalizes variant Arabic characters
    - Alif variants (أ إ آ ٱ) → ا
    - Yeh variants → ى
    - Teh marbuta handling
- **Unicode-aware trigrams**: Substring and typo-tolerant matching within Arabic words
    - `حمد` matches `الحمد`
- **Cross-script search**: Every Arabic word also indexes its Latin transliteration
  and normalized transliteration, so `alhamdu` matches `ٱلْحَمْدُ` — with or
  without diacritics in the source text

### Persian / Urdu / Pashto Support
- **Variant normalization**: letter pairs that look identical but differ by
  codepoint are matched across families, including partial-word matches
    - `ک`/`ك`, `ی`/`ے`/`ي`, `ہ`/`ھ`/`ۃ`/`ه`, `ں`/`ن`
- **Complete transliteration**: Persian/Urdu letters (پ چ ژ گ …) transliterate
  correctly instead of being dropped

### Digit Normalization
- Arabic-Indic (٠-٩) and Extended Arabic-Indic (۰-۹) digits match ASCII digits
  in both directions — `255` finds `٢٥٥` and `۲۵۵`

### Latin / Transliteration Support
- **Latin diacritic folding**: scholarly transliteration matches plain ASCII
    - `ṣalāh` → `salah`, `ʿAẓīm` → `azim` (dotted consonants included)
- **Normalization-based fuzzy matching**: variant spellings meet at a shared
  normalized token
    - `subhanalah`, `subhanallah` and `Subhaanallaahi` match each other
    - `dua` matches `du'a`; `quran` matches `kuran`
- Character n-grams are deliberately **not** emitted for ASCII words — they
  would match every 3-letter fragment and destroy precision

### Indic Script Support
- **Character trigrams** for Bengali, Devanagari and Gujarati: typo-tolerant
  and substring matching

### CJK Support
- **Character bigrams + trigrams** for Han/Kana text (most Chinese words are
  two characters) — unsegmented runs become searchable by any fragment
- **CJK and fullwidth punctuation are word separators** (、。！？ etc.)

### SQL Functions
- `arabic_phonetic_fuzzy_trigram_version()` — which build is loaded (`'0.1.0'`)
- `arabic_phonetic_fuzzy_trigram_tokens(text [, config])` — JSON token dump
  for debugging "why did/didn't this match?"
- `arabic_phonetic_fuzzy_trigram_normalize(word)` — the vocab lookup-key
  transform, routed by script
- `arabic_phonetic_fuzzy_trigram_editdist(a, b [, maxd])` — Levenshtein
  distance over codepoints, for query-time typo correction

### Search Capabilities
- **Prefix search**: `الح*` matches `الحمد`
- **Exact match**: Direct token matching
- **Fuzzy match**: Normalized-spelling equivalence for Latin, trigram overlap
  for Arabic/Indic, bigram overlap for CJK
- **Substring match**: Via character n-gram tokens (non-Latin scripts)
- **Cross-script match**: Latin queries against Arabic text

> `generate_phonetic` (spellfix hashing) is deprecated and ignored since
> 0.1.0 — replaced by `generate_normalized`.

# Getting Started

First, add `sqlite3_arabic_phonetic_fuzzy_trigram_flutter` as a dependency in your pubspec.yaml
file.

```yaml
dependencies:
  sqlite3_arabic_phonetic_fuzzy_trigram_flutter: ^0.2.0
```

The add this in `ios/Podfile` & `macos/Podfile`

```ruby
source 'https://github.com/GreentechApps/cocoapods-specs.git'
source 'https://cdn.cocoapods.org/'
```

Also run this command to add GTAL's pods specs repo

```bash
pod repo add greentech-specs https://github.com/GreentechApps/cocoapods-specs.git
```

Don't forget to `flutter pub get`.