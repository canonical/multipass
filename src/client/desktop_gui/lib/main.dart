import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:window_manager/window_manager.dart';

import 'shell_terminal.dart';
import 'sidebar.dart';

main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await windowManager.ensureInitialized();

  final shellInto = Platform.environment['MULTIPASS_SHELL_INTO'];

  final windowOptions = WindowOptions(title: shellInto ?? 'Multipass');
  await windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });

  if (shellInto != null) {
    runApp(MaterialApp(home: ShellTerminal(instance: shellInto)));
    return;
  }

  runApp(ProviderScope(
    child: MaterialApp(
      theme: ThemeData(
        useMaterial3: true,
        colorSchemeSeed: Colors.white,
      ),
      home: const App(),
    ),
  ));
}

class App extends ConsumerWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final sidebarKey = ref.watch(sidebarKeyProvider);

    return Row(children: [
      const SideBar(),
      Expanded(child: sidebarWidgets[sidebarKey]!),
    ]);
  }
}
