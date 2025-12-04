#include "include/sqlite3_arabic_phonetic_fuzzy_trigram_flutter/sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.h"

void Sqlite3ArabicPhoneticFuzzyTrigramFlutterPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  sqlite3_arabic_phonetic_fuzzy_trigram_flutter::Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
