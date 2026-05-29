import 'dart:ui' show Locale;

import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/main.dart' as app;
import 'package:multipass_gui/providers.dart';

/// Launches the app under test with [overrides] applied, pinned to the English locale.
///
/// Replace [vmInfosStreamProvider] with a cancellation-aware polling override.
///
/// Use this instead of calling [app.main] via [WidgetTester.runAsync] directly.
Future<void> launchApp(
  WidgetTester tester,
  List<dynamic> overrides,
) async {
  tester.binding.platformDispatcher.localesTestValue = [const Locale('en')];
  await tester.runAsync(() async {
    await app.main([
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
