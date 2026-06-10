import 'dart:ui' show Locale;

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/main.dart' as app;
import 'package:multipass_gui/providers.dart';

/// Launches the app under test with [overrides] applied, pinned to the English locale.
///
/// Replace [vmInfosStreamProvider] with a cancellation-aware polling override.
///
/// Use this instead of calling [app.main] via [WidgetTester.runAsync] directly.
///
/// Set [overrideDaemonAvailable] to `false` for tests that need to exercise
/// the [daemonAvailableProvider] logic (e.g. the daemon-unavailable overlay).
/// This avoids a duplicate override conflict and instead pins
/// [ffiAvailableProvider] to `true` so the real [daemonAvailableProvider]
/// can reflect the gRPC stream state.
Future<void> launchApp(
  WidgetTester tester,
  List<dynamic> overrides, {
  bool overrideDaemonAvailable = true,
}) async {
  tester.binding.platformDispatcher.localesTestValue = [const Locale('en')];
  await tester.runAsync(() async {
    await app.main([
      // In tests the gRPC client is always provided via grpcClientProvider
      // override, so the daemon is always reachable regardless of FFI state.
      if (overrideDaemonAvailable)
        daemonAvailableProvider.overrideWith((_) => true)
      else
        // Pin FFI to available so the real daemonAvailableProvider logic runs
        // and can reflect the current vmInfosStreamProvider error state.
        ffiAvailableProvider.overrideWithValue(true),
      // Cancellation-aware polling override avoids timer leaks between tests.
      vmInfosStreamProvider.overrideWith((ref) async* {
        var cancelled = false;
        ref.onDispose(() => cancelled = true);
        final grpcClient = ref.watch(grpcClientProvider);
        Object? lastError;
        while (!cancelled) {
          try {
            yield await grpcClient.info();
            lastError = null;
          } catch (error, stackTrace) {
            if (!cancelled && error != lastError) {
              yield* Stream.error(error, stackTrace);
            }
            lastError = error;
          }
          for (var i = 0; i < 10 && !cancelled; i++) {
            await Future.delayed(const Duration(milliseconds: 100));
          }
        }
      }),
      ...overrides,
    ]);
  });
  addTearDown(() async {
    providerContainer.dispose();
    await tester.pump();
  });
}

/// Pumps the widget tree at [interval] until [finder] matches at least one
/// widget, or [timeout] elapses.
///
/// Set [settle] to `false` to skip the final [WidgetTester.pumpAndSettle] —
/// useful when the matched widget contains a continuous animation (e.g. a
/// spinner) that would prevent settling.
Future<void> pumpUntil(
  WidgetTester tester,
  Finder finder, {
  Duration timeout = const Duration(seconds: 5),
  Duration interval = const Duration(milliseconds: 100),
  bool settle = true,
}) async {
  final end = tester.binding.clock.now().add(timeout);
  while (finder.evaluate().isEmpty) {
    if (tester.binding.clock.now().isAfter(end)) {
      throw Exception('pumpUntil timed out waiting for $finder');
    }
    await tester.pump(interval);
  }
  if (settle) await tester.pumpAndSettle();
}

/// Pump the widget tree at [interval] until [finder] matches no widgets,
/// or [timeout] elapses.
///
/// Set [settle] to `false` to skip the final [WidgetTester.pumpAndSettle].
Future<void> pumpUntilGone(
  WidgetTester tester,
  Finder finder, {
  Duration timeout = const Duration(seconds: 5),
  Duration interval = const Duration(milliseconds: 100),
  bool settle = true,
}) async {
  final end = tester.binding.clock.now().add(timeout);
  while (finder.evaluate().isNotEmpty) {
    if (tester.binding.clock.now().isAfter(end)) {
      throw Exception('pumpUntilGone timed out waiting for $finder to vanish');
    }
    await tester.pump(interval);
  }
  if (settle) await tester.pumpAndSettle();
}
