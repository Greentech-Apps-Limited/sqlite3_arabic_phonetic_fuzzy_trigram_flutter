import 'package:flutter_test/flutter_test.dart';
import 'package:sqlite3_arabic_phonetic_fuzzy_trigram_flutter/sqlite3_arabic_phonetic_fuzzy_trigram_flutter.dart';
import 'package:sqlite3_arabic_phonetic_fuzzy_trigram_flutter/sqlite3_arabic_phonetic_fuzzy_trigram_flutter_platform_interface.dart';
import 'package:sqlite3_arabic_phonetic_fuzzy_trigram_flutter/sqlite3_arabic_phonetic_fuzzy_trigram_flutter_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockSqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform
    with MockPlatformInterfaceMixin
    implements Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform {

  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

void main() {
  final Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform initialPlatform = Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform.instance;

  test('$MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter>());
  });

  test('getPlatformVersion', () async {
    Sqlite3ArabicPhoneticFuzzyTrigramFlutter sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin = Sqlite3ArabicPhoneticFuzzyTrigramFlutter();
    MockSqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform fakePlatform = MockSqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform();
    Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform.instance = fakePlatform;

    expect(await sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin.getPlatformVersion(), '42');
  });
}
