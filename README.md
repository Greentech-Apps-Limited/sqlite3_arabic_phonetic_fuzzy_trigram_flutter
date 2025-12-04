# sqlite3_arabic_phonetic_fuzzy_trigram_flutter

To use [sqlite3-arabic-phonetic-fuzzy-trigram](https://github.com/Greentech-Apps-Limited/sqlite3-arabic-phonetic-fuzzy-trigram) in Flutter.

# sqlite3-arabic-phonetic-fuzzy-trigram

A custom SQLite FTS5 tokenizer designed for Arabic and Latin text with diacritics support, phonetic matching, and fuzzy search capabilities.

## Features

### Arabic Text Support
- **Diacritic-insensitive search**: Matches Arabic text with or without diacritics (tashkeel)
    - `الحمد` matches `ٱلْحَمْدُ`
- **Character normalization**: Normalizes variant Arabic characters
    - Alif variants (أ إ آ ٱ) → ا
    - Yeh variants → ى
    - Teh marbuta handling
- **Unicode-aware trigrams**: Enables substring matching within Arabic words
    - `حمد` matches `الحمد`

### Latin/Transliteration Support
- **Latin diacritic normalization**: Removes diacritics from Latin characters
    - `ṣalāh` → `salah`
- **Phonetic hashing**: Fuzzy matching for Latin text using phonetic patterns
    - `salah` matches `salaah`
    - `quran` matches `kuran`
- **Byte-based trigrams**: Partial/fuzzy matching for Latin words

### Search Capabilities
- **Prefix search**: `الح*` matches `الحمد`
- **Exact match**: Direct token matching
- **Fuzzy match**: Phonetic similarity for Latin text
- **Substring match**: Via trigram tokens

# Getting Started

First, add `sqlite3_arabic_phonetic_fuzzy_trigram_flutter` as a dependency in your pubspec.yaml file.

```yaml
dependencies:
  sqlite3_arabic_phonetic_fuzzy_trigram_flutter: ^0.0.1
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