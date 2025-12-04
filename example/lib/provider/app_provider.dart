import 'package:flutter/material.dart';
import 'package:sqlite3_arabic_phonetic_fuzzy_trigram_flutter_example/model/content.dart';

import '../repository/app_database.dart';

class AppProvider extends ChangeNotifier {
  final _appDatabase = AppDatabase();

  List<Content> results = [];

  AppProvider() {
    _appDatabase.testTokenizer();
  }

  Future<void> search(String key) async {
    if (key.isEmpty) {
      results = [];
      notifyListeners();
      return;
    }
    results = await _appDatabase.search(key);
    notifyListeners();
  }
}
