import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import '../settings/autostart_notifiers.dart';
import '../vm_details/terminal.dart';
import 'platform.dart';

class LinuxPlatform extends MpPlatform {
  @override
  AutostartNotifier autostartNotifier() => LinuxAutostartNotifier();

  @override
  Map<String, String> get drivers => const {
        'qemu': 'QEMU',
        'lxd': 'LXD',
        'libvirt': 'libvirt',
      };

  @override
  String get ffiLibraryName => 'libdart_ffi.so';

  @override
  bool get showLocalUpdateNotifications => false;

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
  String get trayIconFile => 'icon.png';

  @override
  String get metaKey => 'Super';

  @override
  String? get homeDirectory => Platform.environment['SNAP'] == null
      ? Platform.environment['HOME']
      : Platform.environment['SNAP_REAL_HOME'];
}

class LinuxAutostartNotifier extends AutostartNotifier {
  static const autostartFile = 'multipass.gui.autostart.desktop';
  final file = File(
    '${Platform.environment['HOME']}/.config/autostart/$autostartFile',
  );

  LinuxAutostartNotifier() {
    if (FileSystemEntity.isLinkSync(file.path)) {
      Link(file.path).deleteSync();
    }
  }

  @override
  Future<bool> build() => file.exists();

  @override
  Future<void> doSet(bool value) async {
    if (value) {
      final data = await rootBundle.load('assets/$autostartFile');
      await file.parent.create();
      await file.writeAsBytes(data.buffer.asUint8List());
    } else {
      if (await file.exists()) await file.delete();
    }
  }
}
