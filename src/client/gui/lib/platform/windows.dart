import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import '../settings/autostart_notifiers.dart';
import 'platform.dart';

class WindowsPlatform extends MpPlatform {
  @override
  AutostartNotifier autostartNotifier() => WindowsAutostartNotifier();

  @override
  Map<String, String> get drivers => const {
        'hyperv': 'Hyper-V',
        'virtualbox': 'VirtualBox',
      };

  @override
  String get ffiLibraryName => 'dart_ffi.dll';

  @override
  bool get showToggleWindow => true;

  @override
  Map<SingleActivator, Intent> get terminalShortcuts => const {
        SingleActivator(LogicalKeyboardKey.keyC, control: true, shift: true):
            CopySelectionTextIntent.copy,
        SingleActivator(LogicalKeyboardKey.keyV, control: true, shift: true):
            PasteTextIntent(SelectionChangedCause.keyboard),
      };

  @override
  String get trayIconFile => 'icon.ico';
}

class WindowsAutostartNotifier extends AutostartNotifier {
  final link = Link(
    '${Platform.environment['AppData']}/Microsoft/Windows/Start Menu/Programs/Startup/Multipass.lnk',
  );

  @override
  Future<bool> build() => link.exists();

  @override
  Future<void> doSet(bool value) async {
    if (value) {
      await link.create(Platform.resolvedExecutable);
    } else {
      if (await link.exists()) await link.delete();
    }
  }
}
