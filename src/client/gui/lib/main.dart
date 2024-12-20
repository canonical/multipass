import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hotkey_manager/hotkey_manager.dart';
import 'package:local_notifier/local_notifier.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:window_manager/window_manager.dart';

import 'before_quit_dialog.dart';
import 'catalogue/catalogue.dart';
import 'daemon_unavailable.dart';
import 'help.dart';
import 'logger.dart';
import 'notifications.dart';
import 'providers.dart';
import 'settings/hotkey.dart';
import 'settings/settings.dart';
import 'sidebar.dart';
import 'tray_menu.dart';
import 'vm_details/mapping_slider.dart';
import 'vm_details/vm_details.dart';
import 'vm_table/vm_table_screen.dart';
import 'window_size.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await setupLogger();

  await localNotifier.setup(
    appName: 'Multipass',
    shortcutPolicy: ShortcutPolicy.requireCreate, // Only for Windows
  );

  final sharedPreferences = await SharedPreferences.getInstance();
  await windowManager.ensureInitialized();
  final windowOptions = WindowOptions(
    center: true,
    minimumSize: const Size(750, 450),
    size: await deriveWindowSize(sharedPreferences),
    title: 'Multipass',
  );

  await windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });

  await hotKeyManager.unregisterAll();

  providerContainer = ProviderContainer(overrides: [
    guiSettingProvider.overrideWith(() {
      return GuiSettingNotifier(sharedPreferences);
    }),
  ]);
  setupTrayMenu(providerContainer);
  runApp(
    UncontrolledProviderScope(
      container: providerContainer,
      child: MaterialApp(theme: theme, home: const App()),
    ),
  );
}

class App extends ConsumerStatefulWidget {
  const App({super.key});

  @override
  ConsumerState<App> createState() => _AppState();
}

class _AppState extends ConsumerState<App> with WindowListener {
  @override
  Widget build(BuildContext context) {
    final currentKey = ref.watch(sidebarKeyProvider);
    final sidebarExpanded = ref.watch(sidebarExpandedProvider);
    final sidebarPushContent = ref.watch(sidebarPushContentProvider);
    final vms = ref.watch(vmNamesProvider);

    final widgets = {
      CatalogueScreen.sidebarKey: const CatalogueScreen(),
      VmTableScreen.sidebarKey: const VmTableScreen(),
      SettingsScreen.sidebarKey: const SettingsScreen(),
      HelpScreen.sidebarKey: const HelpScreen(),
      for (final name in vms) 'vm-$name': VmDetailsScreen(name),
    };

    final content = Stack(
      fit: StackFit.expand,
      children: widgets.entries.map((e) {
        final MapEntry(:key, value: widget) = e;
        final isCurrent = key == currentKey;
        var maintainState = key != SettingsScreen.sidebarKey;
        if (key.startsWith('vm-')) {
          maintainState = ref.read(vmVisitedProvider(key));
        }
        return Visibility(
          key: Key(key),
          maintainState: maintainState,
          visible: isCurrent,
          child: FocusScope(
            autofocus: isCurrent,
            canRequestFocus: isCurrent,
            skipTraversal: !isCurrent,
            child: widget,
          ),
        );
      }).toList(),
    );

    final hotkey = ref.watch(hotkeyProvider);

    return Stack(children: [
      AnimatedPositioned(
        duration: SideBar.animationDuration,
        bottom: 0,
        right: 0,
        top: 0,
        left: sidebarPushContent && sidebarExpanded
            ? SideBar.expandedWidth
            : SideBar.collapsedWidth,
        child: content,
      ),
      CallbackGlobalShortcuts(
        key: hotkey != null ? GlobalObjectKey(hotkey) : null,
        bindings: {if (hotkey != null) hotkey: goToPrimary},
        child: const SideBar(),
      ),
      const Align(
        alignment: Alignment.bottomRight,
        child: SizedBox(width: 400, child: NotificationList()),
      ),
      const DaemonUnavailable(),
    ]);
  }

  void goToPrimary() {
    final vms = ref.read(vmNamesProvider);
    final primary = ref.read(clientSettingProvider(primaryNameKey));
    if (vms.contains(primary)) {
      ref.read(sidebarKeyProvider.notifier).set('vm-$primary');
      windowManager.showAndRestore();
    }
  }

  @override
  void initState() {
    super.initState();
    windowManager.addListener(this);
    windowManager.setPreventClose(true);
  }

  @override
  void dispose() {
    windowManager.removeListener(this);
    super.dispose();
  }

  // this event handler is called continuously during a window resizing operation
  // so we want to save the data to the disk only after the resizing stops
  @override
  void onWindowResize() => saveWindowSizeTimer.reset();

  @override
  void onWindowClose() async {
    if (!await windowManager.isPreventClose()) return;
    final daemonAvailable = ref.read(daemonAvailableProvider);
    final vmsRunning =
        ref.read(vmStatusesProvider).values.contains(Status.RUNNING);
    if (!daemonAvailable || !vmsRunning) windowManager.destroy();

    stopAllInstances() {
      final notificationsNotifier = ref.read(notificationsProvider.notifier);
      notificationsNotifier.addOperation(
        ref.read(grpcClientProvider).stop([]),
        loading: 'Stopping all instances',
        onError: (error) => 'Failed to stop all instances: $error',
        onSuccess: (_) {
          windowManager.destroy();
          return 'Stopped all instances';
        },
      );
    }

    switch (ref.read(guiSettingProvider(onAppCloseKey))) {
      case 'nothing':
        windowManager.destroy();
      case 'stop':
        stopAllInstances();
      default:
        showDialog(
          context: context,
          barrierDismissible: false,
          builder: (context) => BeforeQuitDialog(
            onStop: (remember) {
              ref
                  .read(guiSettingProvider(onAppCloseKey).notifier)
                  .set(remember ? 'stop' : 'ask');
              stopAllInstances();
              Navigator.pop(context);
            },
            onKeep: (remember) {
              ref
                  .read(guiSettingProvider(onAppCloseKey).notifier)
                  .set(remember ? 'nothing' : 'ask');
              windowManager.destroy();
            },
            onClose: () => Navigator.pop(context),
          ),
        );
    }
  }
}

final theme = ThemeData(
  useMaterial3: false,
  fontFamily: 'Ubuntu',
  fontFamilyFallback: ['NotoColorEmoji', 'FreeSans'],
  inputDecorationTheme: const InputDecorationTheme(
    contentPadding: EdgeInsets.symmetric(vertical: 16, horizontal: 6),
    fillColor: Color(0xfff2f2f2),
    filled: true,
    focusedBorder: UnderlineInputBorder(
      borderSide: BorderSide(width: 2),
      borderRadius: BorderRadius.zero,
    ),
    enabledBorder: UnderlineInputBorder(
      borderSide: BorderSide(width: 2),
      borderRadius: BorderRadius.zero,
    ),
    isDense: true,
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
      textStyle: const TextStyle(fontFamily: 'Ubuntu', fontSize: 16),
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
      textStyle: const TextStyle(fontFamily: 'Ubuntu', fontSize: 16),
    ),
  ),
  textSelectionTheme: const TextSelectionThemeData(
    cursorColor: Colors.black,
    selectionColor: Colors.grey,
  ),
  tabBarTheme: const TabBarTheme(
    indicator: BoxDecoration(
      color: Colors.black12,
      border: Border(bottom: BorderSide(width: 3)),
    ),
    indicatorSize: TabBarIndicatorSize.tab,
    labelColor: Colors.black,
    labelStyle: TextStyle(
      fontFamily: 'Ubuntu',
      fontWeight: FontWeight.bold,
    ),
    unselectedLabelColor: Colors.black,
    unselectedLabelStyle: TextStyle(
      fontFamily: 'Ubuntu',
      fontWeight: FontWeight.bold,
    ),
    tabAlignment: TabAlignment.start,
  ),
  sliderTheme: SliderThemeData(
    activeTrackColor: Color(0xff0066cc),
    inactiveTrackColor: Color(0xffd9d9d9),
    overlayShape: SliderComponentShape.noThumb,
    thumbColor: Colors.white,
    thumbShape: CustomThumbShape(),
    tickMarkShape: SliderTickMarkShape.noTickMark,
    trackHeight: 2,
    trackShape: CustomTrackShape(),
  ),
);
