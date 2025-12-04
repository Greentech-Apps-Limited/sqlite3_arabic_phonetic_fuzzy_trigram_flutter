import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:sqlite3_arabic_phonetic_fuzzy_trigram_flutter/sqlite3_arabic_phonetic_fuzzy_trigram_flutter_method_channel.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter platform = MethodChannelSqlite3ArabicPhoneticFuzzyTrigramFlutter();
  const MethodChannel channel = MethodChannel('sqlite3_arabic_phonetic_fuzzy_trigram_flutter');

  setUp(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      channel,
      (MethodCall methodCall) async {
        return '42';
      },
    );
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(channel, null);
  });

  test('getPlatformVersion', () async {
    expect(await platform.getPlatformVersion(), '42');
  });
}
