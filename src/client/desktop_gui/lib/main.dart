import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'launch_panel.dart';
import 'vms_screen.dart';

main() {
  runApp(ProviderScope(
    child: MaterialApp(
      title: 'Multipass',
      theme: ThemeData(
        useMaterial3: true,
        colorSchemeSeed: Colors.white,
      ),
      home: Scaffold(
        body: const VMsScreen(),
        drawerScrimColor: Colors.transparent,
        endDrawer: Drawer(
          surfaceTintColor: Colors.transparent,
          width: 420,
          elevation: 5,
          shadowColor: Colors.black,
          shape: const Border(),
          child: LaunchPanel(),
        ),
      ),
    ),
  ));
}
