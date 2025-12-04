import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'sqlite3_arabic_phonetic_fuzzy_trigram_flutter_method_channel.dart';

abstract class Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform extends PlatformInterface {
  /// Constructs a Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform.
  Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform() : super(token: _token);

  static final Object _token = Object();

  static Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform _instance = MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter();

  /// The default instance of [Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform] to use.
  ///
  /// Defaults to [MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter].
  static Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform] when
  /// they register themselves.
  static set instance(Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<bool> loadNativeLibrary() {
    throw UnimplementedError('loadNativeLibrary() has not been implemented.');
  }
}
