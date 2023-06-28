import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'ffi.dart';
import 'grpc_client.dart';

void main() {
  runApp(const ProviderScope(child: MaterialApp(home: MyHomePage())));
}

class MyHomePage extends ConsumerWidget {
  const MyHomePage({super.key});

  Widget _buildInfosColumn(Iterable<VmInfo> infos) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: infos
          .map((info) => Text('${info.name} ${info.instanceStatus.status}'))
          .toList(),
    );
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final instances = ref.watch(vmInfosStreamProvider).when(
          data: _buildInfosColumn,
          error: (error, _) => Text('$error'),
          loading: () => const Text('Loading...'),
        );
    return Scaffold(
      body: Column(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [instances, Text(multipassVersion)],
      ),
    );
  }
}
