import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'globals.dart';
import 'grpc_client.dart';

class InstanceActionsButton extends ConsumerWidget {
  final Status status;
  final String name;

  const InstanceActionsButton(this.status, this.name, {super.key});

  Map<String, void Function()> actions(GrpcClient client) {
    return {
      if (status == Status.RUNNING) 'Stop': () => client.stop([name]),
      if (status == Status.RUNNING) 'Suspend': () => client.suspend([name]),
      if (status == Status.RUNNING) 'Restart': () => client.restart([name]),
      if (status == Status.DELETED) 'Recover': () => client.recover([name]),
      if (status == Status.DELETED) 'Purge': () => client.purge([name]),
      if ([Status.STOPPED, Status.SUSPENDED].contains(status))
        'Start': () => client.start([name]),
      if (status != Status.DELETED) 'Delete': () => client.delete([name]),
    };
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) => PopupMenuButton<void>(
        icon: const Icon(Icons.more_vert),
        splashRadius: 20,
        itemBuilder: (_) => actions(ref.watch(grpcClient))
            .entries
            .map((e) => PopupMenuItem(onTap: e.value, child: Text(e.key)))
            .toList(),
      );
}
