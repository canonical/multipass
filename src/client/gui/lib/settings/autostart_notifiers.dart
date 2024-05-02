import 'dart:async';
import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final autostartProvider =
    AsyncNotifierProvider.autoDispose<AutostartNotifier, bool>(() {
  if (Platform.isLinux) return LinuxAutostartNotifier();
  if (Platform.isWindows) return WindowsAutostartNotifier();
  if (Platform.isMacOS) return MacOSAutostartNotifier();
  throw UnimplementedError('no autostart notifier for this platform');
});

abstract class AutostartNotifier extends AutoDisposeAsyncNotifier<bool> {
  Future<void> set(bool value) async {
    try {
      await _doSet(value);
    } finally {
      ref.invalidateSelf();
    }
  }

  Future<void> _doSet(bool value);
}

class LinuxAutostartNotifier extends AutostartNotifier {
  static const autostartFile = 'multipass.gui.autostart.desktop';
  final file = File(
    '${Platform.environment['HOME']}/.config/autostart/$autostartFile',
  );

  @override
  Future<bool> build() => file.exists();

  @override
  Future<void> _doSet(bool value) async {
    if (value) {
      final data = await rootBundle.load('assets/$autostartFile');
      await file.writeAsBytes(data.buffer.asUint8List());
    } else {
      if (await file.exists()) await file.delete();
    }
  }
}

class WindowsAutostartNotifier extends AutostartNotifier {
  final link = Link(
    '${Platform.environment['AppData']}/Microsoft/Windows/Start Menu/Programs/Startup/Multipass.lnk',
  );

  @override
  Future<bool> build() => link.exists();

  @override
  Future<void> _doSet(bool value) async {
    if (value) {
      await link.create(Platform.resolvedExecutable);
    } else {
      if (await link.exists()) await link.delete();
    }
  }
}

class MacOSAutostartNotifier extends AutostartNotifier {
  static const plistFile = 'com.canonical.multipass.gui.autostart.plist';
  final file = File(
    '${Platform.environment['HOME']}/Library/LaunchAgents/$plistFile',
  );

  @override
  Future<bool> build() => file.exists();

  @override
  Future<void> _doSet(bool value) async {
    if (value) {
      final data = await rootBundle.load('assets/$plistFile');
      await file.writeAsBytes(data.buffer.asUint8List());
    } else {
      if (await file.exists()) await file.delete();
    }
  }
}
