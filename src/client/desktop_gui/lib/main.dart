import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'vms_screen.dart';

main() {
  runApp(ProviderScope(
    child: MaterialApp(
      title: 'Multipass',
      theme: ThemeData(useMaterial3: true, colorSchemeSeed: Colors.blue),
      home: const Scaffold(body: VMsScreen()),
    ),
  ));
}
