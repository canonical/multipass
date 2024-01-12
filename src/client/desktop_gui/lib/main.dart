import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:window_manager/window_manager.dart';

import 'sidebar.dart';
import 'tray_menu.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await windowManager.ensureInitialized();

  const windowOptions = WindowOptions(size: Size(1400, 800));
  await windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });

  final providerContainer = ProviderContainer();
  setupTrayMenu(providerContainer);
  runApp(
    UncontrolledProviderScope(
      container: providerContainer,
      child: const App(),
    ),
  );
}

class App extends ConsumerWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final content = ref.watch(sidebarWidgetProvider);

    return MaterialApp(
      theme: theme,
      home: SideBar(child: content),
    );
  }
}

final theme = ThemeData(
  fontFamily: 'Ubuntu',
  inputDecorationTheme: const InputDecorationTheme(
    border: OutlineInputBorder(borderRadius: BorderRadius.zero),
    isDense: true,
    focusedBorder: OutlineInputBorder(
      borderRadius: BorderRadius.zero,
      borderSide: BorderSide(),
    ),
    suffixIconColor: Colors.black,
  ),
  outlinedButtonTheme: OutlinedButtonThemeData(
    style: OutlinedButton.styleFrom(
      disabledForegroundColor: Colors.black.withOpacity(0.5),
      foregroundColor: Colors.black,
      padding: const EdgeInsets.all(16),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(2),
      ),
      side: const BorderSide(color: Color(0xff333333)),
      textStyle: const TextStyle(
        fontSize: 16,
        fontWeight: FontWeight.w300,
      ),
    ),
  ),
  scaffoldBackgroundColor: Colors.white,
  textButtonTheme: TextButtonThemeData(
    style: TextButton.styleFrom(
      backgroundColor: const Color(0xff0E8620),
      disabledForegroundColor: Colors.white.withOpacity(0.5),
      foregroundColor: Colors.white,
      padding: const EdgeInsets.all(16),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(2),
      ),
      textStyle: const TextStyle(
        fontSize: 16,
        fontWeight: FontWeight.w300,
      ),
    ),
  ),
  textSelectionTheme: const TextSelectionThemeData(
    cursorColor: Colors.black,
    selectionColor: Colors.grey,
  ),
);
