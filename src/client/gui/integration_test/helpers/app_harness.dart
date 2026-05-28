import 'dart:ui' show Locale;

import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/main.dart' as app;

/// Launches the app under test with [overrides] applied, pinned to the English
/// locale.
///
/// Use this instead of calling [app.main] via [WidgetTester.runAsync] directly.
Future<void> launchApp(
  WidgetTester tester,
  List<dynamic> overrides,
) async {
  tester.binding.platformDispatcher.localesTestValue = [const Locale('en')];
  await tester.runAsync(() async {
    await app.main([...overrides]);
  });
}
