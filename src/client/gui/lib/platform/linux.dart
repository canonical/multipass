import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import '../settings/autostart_notifiers.dart';
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
  bool get showToggleWindow => true;

  @override
  Map<SingleActivator, Intent> get terminalShortcuts => const {
        SingleActivator(LogicalKeyboardKey.keyC, control: true, shift: true):
            CopySelectionTextIntent.copy,
        SingleActivator(LogicalKeyboardKey.keyV, control: true, shift: true):
            PasteTextIntent(SelectionChangedCause.keyboard),
      };

  @override
  String get trayIconFile => 'icon.png';

  @override
  String get metaKey => 'Super';
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
