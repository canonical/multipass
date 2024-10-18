//
//  Generated file. Do not edit.
//

import FlutterMacOS
import Foundation

import file_selector_macos
import hotkey_manager_macos
import local_notifier
import path_provider_foundation
import screen_retriever
import shared_preferences_foundation
import tray_menu
import url_launcher_macos
import window_manager
import window_size

func RegisterGeneratedPlugins(registry: FlutterPluginRegistry) {
  FileSelectorPlugin.register(with: registry.registrar(forPlugin: "FileSelectorPlugin"))
  HotkeyManagerMacosPlugin.register(with: registry.registrar(forPlugin: "HotkeyManagerMacosPlugin"))
  LocalNotifierPlugin.register(with: registry.registrar(forPlugin: "LocalNotifierPlugin"))
  PathProviderPlugin.register(with: registry.registrar(forPlugin: "PathProviderPlugin"))
  ScreenRetrieverPlugin.register(with: registry.registrar(forPlugin: "ScreenRetrieverPlugin"))
  SharedPreferencesPlugin.register(with: registry.registrar(forPlugin: "SharedPreferencesPlugin"))
  TrayMenuPlugin.register(with: registry.registrar(forPlugin: "TrayMenuPlugin"))
  UrlLauncherPlugin.register(with: registry.registrar(forPlugin: "UrlLauncherPlugin"))
  WindowManagerPlugin.register(with: registry.registrar(forPlugin: "WindowManagerPlugin"))
  WindowSizePlugin.register(with: registry.registrar(forPlugin: "WindowSizePlugin"))
}
