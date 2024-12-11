import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import '../settings/autostart_notifiers.dart';
import '../vm_details/terminal.dart';
import 'platform.dart';

class MacOSPlatform extends MpPlatform {
  @override
  AutostartNotifier autostartNotifier() => MacOSAutostartNotifier();

  @override
  Map<String, String> get drivers => const {
        'qemu': 'QEMU',
        'virtualbox': 'VirtualBox',
      };

  @override
  String get ffiLibraryName => 'libdart_ffi.dylib';

  @override
  bool get showLocalUpdateNotifications => true;

  @override
  bool get showToggleWindow => false;

  @override
  Map<SingleActivator, Intent> get terminalShortcuts => const {
        SingleActivator(LogicalKeyboardKey.keyC, meta: true):
            CopySelectionTextIntent.copy,
        SingleActivator(LogicalKeyboardKey.keyV, meta: true):
            PasteTextIntent(SelectionChangedCause.keyboard),
        SingleActivator(LogicalKeyboardKey.equal, meta: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.equal, meta: true, shift: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.add, meta: true, shift: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.numpadAdd, meta: true):
            IncreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.minus, meta: true):
            DecreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.numpadSubtract, meta: true):
            DecreaseTerminalFontIntent(),
        SingleActivator(LogicalKeyboardKey.digit0, meta: true):
            ResetTerminalFontIntent(),
      };

  @override
  String get trayIconFile => 'icon_template.png';

  @override
  String get metaKey => 'Command';

  @override
  String get altKey => 'Option';

  @override
  String? get homeDirectory => Platform.environment['HOME'];
}

class MacOSAutostartNotifier extends AutostartNotifier {
  static const plistFile = 'com.canonical.multipass.gui.autostart.plist';
  final file = File(
    '${Platform.environment['HOME']}/Library/LaunchAgents/$plistFile',
  );

  @override
  Future<bool> build() => file.exists();

  @override
  Future<void> doSet(bool value) async {
    if (value) {
      final data = await rootBundle.load('assets/$plistFile');
      await file.writeAsBytes(data.buffer.asUint8List());
    } else {
      if (await file.exists()) await file.delete();
    }
  }
}
