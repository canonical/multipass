import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hotkey_manager/hotkey_manager.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:window_manager/window_manager.dart';

import 'catalogue/catalogue.dart';
import 'daemon_unavailable.dart';
import 'help.dart';
import 'logger.dart';
import 'providers.dart';
import 'settings/settings.dart';
import 'sidebar.dart';
import 'tray_menu.dart';
import 'vm_details/vm_details.dart';
import 'vm_table/vm_table_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  FlutterError.onError = (details) {
    logger.e(
      'Flutter error',
      error: details.exception,
      stackTrace: details.stack,
    );
  };
  PlatformDispatcher.instance.onError = (error, stack) {
    logger.e(
      'Dart error',
      error: error,
      stackTrace: stack,
    );
    return true;
  };

  await setupLogger();

  await windowManager.ensureInitialized();
  const windowOptions = WindowOptions(
    size: Size(1400, 800),
    title: 'Multipass',
  );
  await windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });

  await hotKeyManager.unregisterAll();
  final sharedPreferences = await SharedPreferences.getInstance();

  final providerContainer = ProviderContainer(overrides: [
    guiSettingProvider.overrideWith(() {
      return GuiSettingNotifier(sharedPreferences);
    }),
  ]);
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
    final currentKey = ref.watch(sidebarKeyProvider);
    final daemonAvailable = ref.watch(daemonAvailableProvider);
    final vms = ref.watch(vmNamesProvider);

    final widgets = {
      CatalogueScreen.sidebarKey: const CatalogueScreen(),
      VmTableScreen.sidebarKey: const VmTableScreen(),
      SettingsScreen.sidebarKey: const SettingsScreen(),
      HelpScreen.sidebarKey: const HelpScreen(),
      for (final name in vms) 'vm-$name': VmDetailsScreen(name: name),
    };

    final content = Stack(fit: StackFit.expand, children: [
      for (final MapEntry(:key, value: widget) in widgets.entries)
        Visibility(
          maintainState: true,
          visible: key == currentKey,
          child: widget,
        ),
    ]);

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
  useMaterial3: false,
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
