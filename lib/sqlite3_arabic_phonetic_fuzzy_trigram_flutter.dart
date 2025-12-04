import 'dart:ffi';
import 'dart:io';
import 'dart:math';

import 'sqlite3_arabic_phonetic_fuzzy_trigram_flutter_platform_interface.dart';

class Sqlite3ArabicPhoneticFuzzyTrigramFlutter {
  Future<DynamicLibrary?> loadNativeLibrary() async {
    if (Platform.isAndroid) {
      try {
        return Future.value(
          DynamicLibrary.open('libarabicphoneticfuzzytrigram.so'),
        );
      } on ArgumentError {
        // On some (especially old) Android devices, we somehow can't dlopen
        // libraries shipped with the apk. We need to find the full path of the
        // library (/data/data/<id>/lib/libsqlite3.so) and open that one.
        // For details, see https://github.com/simolus3/moor/issues/420
        final appIdAsBytes = File('/proc/self/cmdline').readAsBytesSync();

        // app id ends with the first \0 character in here.
        final endOfAppId = max(appIdAsBytes.indexOf(0), 0);
        final appId = String.fromCharCodes(appIdAsBytes.sublist(0, endOfAppId));

        return Future.value(
          DynamicLibrary.open(
            '/data/data/$appId/lib/libsqlite3_arabic_phonetic_fuzzy_trigram.so',
          ),
        );
      } catch (_) {
        final status = await Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlatform
            .instance
            .loadNativeLibrary();
        if (status) {
          return Future.value(
            DynamicLibrary.open('libarabicphoneticfuzzytrigram.so'),
          );
        }
      }
    } else if (Platform.isWindows) {
      return Future.value(
        DynamicLibrary.open(
          'sqlite3_arabic_phonetic_fuzzy_trigram_flutter_plugin.dll',
        ),
      );
    } else if (Platform.isIOS || Platform.isMacOS) {
      return Future.value(
        DynamicLibrary.open(
          'sqlite3_arabic_phonetic_fuzzy_trigram.framework/sqlite3_arabic_phonetic_fuzzy_trigram',
        ),
      );
    } else if (Platform.isLinux) {
      final library = DynamicLibrary.executable();
      if (library.providesSymbol("sqlite3_arabic_phonetic_fuzzy_trigram_init")) {
        return Future.value(library);
      }
    }
    return Future.value(null);
  }
}
