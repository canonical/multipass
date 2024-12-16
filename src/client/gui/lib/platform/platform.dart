import 'dart:io';

import 'package:flutter/widgets.dart';

import '../settings/autostart_notifiers.dart';
import 'linux.dart';
import 'macos.dart';
import 'windows.dart';

abstract class MpPlatform {
  String get ffiLibraryName;

  AutostartNotifier autostartNotifier();

  Map<String, String> get drivers;

  String get trayIconFile;

  Map<SingleActivator, Intent> get terminalShortcuts;

  bool get showLocalUpdateNotifications;

  bool get showToggleWindow;

  String get altKey => 'Alt';

  String get metaKey => 'Meta';

  String? get homeDirectory;
}

MpPlatform _getPlatform() {
  if (Platform.isLinux) return LinuxPlatform();
  if (Platform.isMacOS) return MacOSPlatform();
  if (Platform.isWindows) return WindowsPlatform();
  throw UnimplementedError('Platform not supported');
}

final mpPlatform = _getPlatform();
