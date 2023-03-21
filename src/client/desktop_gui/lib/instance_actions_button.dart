import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'globals.dart';
import 'grpc_client.dart';
import 'instance_action.dart';
import 'snackbar.dart';

class InstanceActionsButton extends ConsumerWidget {
  final Status status;
  final String name;

  const InstanceActionsButton(this.status, this.name, {super.key});

  List<InstanceAction> actions(GrpcClient client) => [
        InstanceAction(
          name: 'Start',
          instances: [name],
          function: client.start,
        ),
        InstanceAction(
          name: 'Stop',
          instances: [name],
          function: client.stop,
        ),
        InstanceAction(
          name: 'Suspend',
          instances: [name],
          function: client.suspend,
        ),
        InstanceAction(
          name: 'Restart',
          instances: [name],
          function: client.restart,
        ),
        InstanceAction(
          name: 'Delete',
          instances: [name],
          function: client.delete,
        ),
        InstanceAction(
          name: 'Recover',
          instances: [name],
          function: client.recover,
        ),
        InstanceAction(
          name: 'Purge',
          instances: [name],
          function: client.purge,
        ),
      ];

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);

    return PopupMenuButton<void>(
      icon: const Icon(Icons.more_vert),
      splashRadius: 20,
      itemBuilder: (_) => actions(client)
          .map((action) => PopupMenuItem(
                enabled: action.allowedStatuses.contains(status),
                onTap: () => showInstanceActionSnackBar(context, action),
                child: Text(action.name),
              ))
          .toList(),
    );
  }
}
