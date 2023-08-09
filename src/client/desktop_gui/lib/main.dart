import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'ffi.dart';
import 'providers.dart';

void main() {
  runApp(const ProviderScope(child: MaterialApp(home: MyHomePage())));
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

    final primaryName = ref.watch(primaryNameProvider);

    return Scaffold(
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          instances,
          const Spacer(),
          TextField(
            onSubmitted: (value) => setSetting('client.primary-name', value),
          ),
          Text('$multipassVersion primary instance: $primaryName'),
        ],
      ),
    );
  }
}
