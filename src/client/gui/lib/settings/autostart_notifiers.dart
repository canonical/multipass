import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../platform/platform.dart';

final autostartProvider =
    AsyncNotifierProvider.autoDispose<AutostartNotifier, bool>(
        mpPlatform.autostartNotifier);

abstract class AutostartNotifier extends AutoDisposeAsyncNotifier<bool> {
  Future<void> set(bool value) async {
    try {
      await doSet(value);
    } finally {
      ref.invalidateSelf();
    }
  }

  Future<void> doSet(bool value);
}
