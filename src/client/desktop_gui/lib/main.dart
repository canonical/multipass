import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:window_manager/window_manager.dart';

import 'daemon_unavailable.dart';
import 'providers.dart';
import 'sidebar.dart';
import 'tray_menu.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await windowManager.ensureInitialized();

  const windowOptions = WindowOptions(
    size: Size(1400, 800),
    title: 'Multipass',
  );
  await windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });

  final sharedPreferences = await SharedPreferences.getInstance();

  final providerContainer = ProviderContainer(
    overrides: [
      guiSettingProvider.overrideWith(() {
        return GuiSettingNotifier(sharedPreferences);
      }),
    ],
  );
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
    final daemonAvailable = ref.watch(daemonAvailableProvider);

    final content = ref.watch(sidebarWidgetProvider);

    return MaterialApp(
      theme: theme,
      home: Stack(fit: StackFit.expand, children: [
        ColorFiltered(
          colorFilter: ColorFilter.mode(
            Colors.grey,
            daemonAvailable ? BlendMode.dst : BlendMode.saturation,
          ),
          child: SideBar(child: content),
        ),
        DaemonUnavailable(daemonAvailable),
      ]),
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
