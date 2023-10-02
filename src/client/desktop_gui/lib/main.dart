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

class App extends StatelessWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Consumer(
        builder: (_, ref, __) {
          final sidebarKey = ref.watch(sidebarKeyProvider);
          return Row(children: [
            const SideBar(),
            Expanded(child: sidebarWidgets[sidebarKey]!),
          ]);
        },
      ),
      theme: ThemeData(
        fontFamily: 'Ubuntu',
        scaffoldBackgroundColor: Colors.white,
      ),
    );
  }
}
