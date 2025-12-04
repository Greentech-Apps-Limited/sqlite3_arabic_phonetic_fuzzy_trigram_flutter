import Flutter
import UIKit

public class Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin: NSObject, FlutterPlugin {
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "sqlite3_arabic_phonetic_fuzzy_trigram_flutter", binaryMessenger: registrar.messenger())
    let instance = Sqlite3ArabicPhoneticFuzzyTrigramFlutterPlugin()
    registrar.addMethodCallDelegate(instance, channel: channel)
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getPlatformVersion":
      result("iOS " + UIDevice.current.systemVersion)
    default:
      result(FlutterMethodNotImplemented)
    }
  }
}
