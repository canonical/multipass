import 'package:desktop_gui/sidebar.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

main() {
  runApp(ProviderScope(
    child: MaterialApp(
      title: 'Multipass',
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
