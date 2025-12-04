import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'sqlite3_arabic_phonetic_fuzzy_trigram_flutter_platform_interface.dart';

/// An implementation of [Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform] that uses method channels.
class MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter extends Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('sqlite3_arabic_phonetic_fuzzy_trigram_flutter');

  @override
  Future<bool> loadNativeLibrary() async {
    final status = await methodChannel.invokeMethod<bool>('loadNativeLibrary');
    return status ?? false;
  }
}
