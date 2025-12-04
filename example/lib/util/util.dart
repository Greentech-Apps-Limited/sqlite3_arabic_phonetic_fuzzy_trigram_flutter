import 'dart:io';

final bool isSupportedPlatforms = [
  'linux',
  'macos',
  'ios',
  'android',
  'windows',
].contains(Platform.operatingSystem);