#ifndef FLUTTER_PLUGIN_SQLITE3_ARABIC_PHONETIC_FUZZY_TRIGRAM_FLUTTER_PLUGIN_H_
#define FLUTTER_PLUGIN_SQLITE3_ARABIC_PHONETIC_FUZZY_TRIGRAM_FLUTTER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace sqlite3_arabic_phonetic_fuzzy_trigram_flutter {

class Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin();

  virtual ~Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin();

  // Disallow copy and assign.
  Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin(const Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin&) = delete;
  Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin& operator=(const Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace sqlite3_arabic_phonetic_fuzzy_trigram_flutter

#endif  // FLUTTER_PLUGIN_SQLITE3_ARABIC_PHONETIC_FUZZY_TRIGRAM_FLUTTER_PLUGIN_H_
