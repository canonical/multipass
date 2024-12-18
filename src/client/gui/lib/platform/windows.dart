import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:win32/win32.dart';

import '../settings/autostart_notifiers.dart';
import '../vm_details/terminal.dart';
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
  bool get showLocalUpdateNotifications => true;

  @override
  bool get showToggleWindow => true;

  @override
  Map<SingleActivator, Intent> get terminalShortcuts => const {
        SingleActivator(LogicalKeyboardKey.keyC, control: true, shift: true):
            CopySelectionTextIntent.copy,
        SingleActivator(LogicalKeyboardKey.insert, control: true):
            CopySelectionTextIntent.copy,
        SingleActivator(LogicalKeyboardKey.keyV, control: true, shift: true):
            PasteTextIntent(SelectionChangedCause.keyboard),
        SingleActivator(LogicalKeyboardKey.insert, shift: true):
            PasteTextIntent(SelectionChangedCause.keyboard),
        SingleActivator(LogicalKeyboardKey.equal, control: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.equal, control: true, shift: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.add, control: true, shift: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.numpadAdd, control: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.minus, control: true):
            DecreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.numpadSubtract, control: true):
            DecreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.digit0, control: true):
            ResetTerminalFontIntent(),
      };

  @override
  String get trayIconFile => 'icon.ico';

  @override
  String get metaKey => 'Win';

  @override
  String? get homeDirectory => Platform.environment['USERPROFILE'];
}

class WindowsAutostartNotifier extends AutostartNotifier {
  WindowsAutostartNotifier() {
    CoInitializeEx(nullptr, 2);
  }

  final link = File(
    '${Platform.environment['AppData']}/Microsoft/Windows/Start Menu/Programs/Startup/Multipass.lnk',
  );

  @override
  Future<bool> build() => link.exists();

  @override
  Future<void> doSet(bool value) async {
    if (value) {
      _createShortcut(Platform.resolvedExecutable, link.path);
    } else {
      if (await link.exists()) await link.delete();
    }
  }

  void _createShortcut(String path, String linkPath) {
    final shellLink = ShellLink.createInstance();
    final pathUtf16 = path.toNativeUtf16();
    final linkPathUtf16 = linkPath.toNativeUtf16();

    try {
      shellLink.setPath(pathUtf16);
      IPersistFile.from(shellLink).save(linkPathUtf16, TRUE);
    } finally {
      free(pathUtf16);
      free(linkPathUtf16);
    }
  }
}
