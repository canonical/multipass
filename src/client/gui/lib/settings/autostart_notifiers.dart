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
  Future<void> set(bool value);
}

class LinuxAutostartNotifier extends AutostartNotifier {
  static const autostartFile = 'multipass.gui.autostart.desktop';
  final file = File(
    '${Platform.environment['HOME']}/.config/autostart/$autostartFile',
  );

  @override
  Future<bool> build() => file.exists();

  @override
  Future<void> set(bool value) async {
    if (value) {
      final data = await rootBundle.load('assets/$autostartFile');
      await file.writeAsBytes(data.buffer.asUint8List());
    } else {
      if (await file.exists()) await file.delete();
    }

    ref.invalidateSelf();
  }
}

class WindowsAutostartNotifier extends AutostartNotifier {
  @override
  FutureOr<bool> build() {
    // TODO: implement build
    throw UnimplementedError();
  }

  @override
  Future<void> set(bool value) {
    // TODO: implement set
    throw UnimplementedError();
  }
}

class MacOSAutostartNotifier extends AutostartNotifier {
  @override
  FutureOr<bool> build() {
    // TODO: implement build
    throw UnimplementedError();
  }

  @override
  Future<void> set(bool value) {
    // TODO: implement set
    throw UnimplementedError();
  }
}
