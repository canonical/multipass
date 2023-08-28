import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:window_manager/window_manager.dart';

import 'ffi.dart';
import 'providers.dart';
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
      child: const MaterialApp(
        home: MyHomePage(),
      ),
    ),
  );
}

class MyHomePage extends ConsumerWidget {
  const MyHomePage({super.key});

  Widget _buildInfosColumn(BuiltMap<String, Status> infos) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: infos.entries
          .map((info) => Text('${info.key} ${info.value}'))
          .toList(),
    );
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final instances = ref
        .watch(vmInfosStreamProvider.select((data) => data.whenData(
              (infos) => infosToStatusMap(infos).build(),
            )))
        .when(
          data: _buildInfosColumn,
          error: (error, _) => Text('$error'),
          loading: () => const Text('Loading...'),
        );

    final settings = ref.watch(clientSettingsProvider);

    return Scaffold(
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          instances,
          const Spacer(),
          Text(settings.entries.map((e) => '${e.key}: ${e.value}').join('\n')),
          Text(multipassVersion),
        ],
      ),
    );
  }
}
