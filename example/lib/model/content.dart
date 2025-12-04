import 'package:flutter/foundation.dart' show immutable;

@immutable
class Content {
  final int id;
  final String content;

  const Content({required this.id, required this.content});

  factory Content.fromJson(Map<String, dynamic> data) =>
      Content(id: data['rowid'], content: data['content']);

  @override
  String toString() {
    return "$id -> $content";
  }
}
