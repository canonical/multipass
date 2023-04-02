import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

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

    final normalActions = actions(client).map(
      (action) => PopupMenuItem(
        enabled: action.allowedStatuses.contains(status),
        onTap: () => showInstanceActionSnackBar(context, action),
        child: Text(action.name),
      ),
    );

    final shellAction = PopupMenuItem(
      enabled:
          [Status.RUNNING, Status.SUSPENDED, Status.STOPPED].contains(status),
      onTap: () {
        if (status == Status.RUNNING) {
          openShell(name);
        } else {
          showInstanceActionSnackBar(
            context,
            InstanceAction(
              name: 'Start',
              instances: [name],
              function: (names) =>
                  client.start(names).then((_) => openShell(name)),
            ),
          );
        }
      },
      child: const Text('Shell'),
    );

    return PopupMenuButton<void>(
      icon: const Icon(Icons.more_vert),
      splashRadius: 20,
      tooltip: 'Instance actions',
      itemBuilder: (_) => [shellAction, ...normalActions],
    );
  }
}

void openShell(String instanceName) {
  Process.run(
    Platform.resolvedExecutable,
    [],
    environment: {'MULTIPASS_SHELL_INTO': instanceName},
  );
}
