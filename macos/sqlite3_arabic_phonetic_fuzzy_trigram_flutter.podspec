#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint sqlite3_arabic_phonetic_fuzzy_trigram_flutter.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'sqlite3_arabic_phonetic_fuzzy_trigram_flutter'
  s.version          = '0.0.1'
  s.summary          = 'SQLite FTS5 Arabic Phonetic Fuzzy Trigram Tokenizer'
  s.description      = <<-DESC
SQLite FTS5 Arabic Phonetic Fuzzy Trigram Tokenizer
                       DESC
  s.homepage         = 'https://greentechapps.com/'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Greentech Apps Limited' => 'contact@greentechapps.com' }

  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'

  # If your plugin requires a privacy manifest, for example if it collects user
  # data, update the PrivacyInfo.xcprivacy file to describe your plugin's
  # privacy impact, and then uncomment this line. For more information,
  # see https://developer.apple.com/documentation/bundleresources/privacy_manifest_files
  # s.resource_bundles = {'sqlite3_arabic_phonetic_fuzzy_trigram_flutter_privacy' => ['Resources/PrivacyInfo.xcprivacy']}

  s.dependency 'FlutterMacOS'
  s.dependency 'sqlite3-arabic-phonetic-fuzzy-trigram', '0.0.1'

  s.platform = :osx, '10.13'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'
end
