import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import '../vm_action.dart';
import 'vms.dart';

class BulkActionsBar extends ConsumerWidget {
  const BulkActionsBar({super.key});

  Iterable<VmAction> actions(GrpcClient client, Iterable<String> vmNames) {
    return [
      VmAction(name: 'Start', vmNames: vmNames, function: client.start),
      VmAction(name: 'Stop', vmNames: vmNames, function: client.stop),
      VmAction(name: 'Suspend', vmNames: vmNames, function: client.suspend),
      VmAction(name: 'Delete', vmNames: vmNames, function: client.delete),
    ];
  }

  Widget buildButton(VmAction action, Set<Status> statuses) {
    final child = Text(action.name);
    final onPressed = action.allowedStatuses.intersection(statuses).isNotEmpty
        ? () => action.function(action.vmNames)
        : null;

    final button = action.name == 'Delete'
        ? TextButton(
            onPressed: onPressed,
            style: const ButtonStyle(
              backgroundColor: MaterialStatePropertyAll(Color(0xffC7162B)),
            ),
            child: child,
          )
        : OutlinedButton(
            onPressed: onPressed,
            style: ButtonStyle(
              side: MaterialStateBorderSide.resolveWith(
                (states) => BorderSide(
                  color: const Color(0xff333333).withOpacity(
                    states.contains(MaterialState.disabled) ? 0.5 : 1,
                  ),
                ),
              ),
            ),
            child: child,
          );

    return Container(margin: const EdgeInsets.only(right: 8), child: button);
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);
    final vmStatuses = ref.watch(vmStatusesProvider);
    final selectedVms = ref.watch(selectedVmsProvider);
    final statuses = vmStatuses.asMap().whereKey(selectedVms.contains).values;

    return Row(
      children: actions(client, selectedVms)
          .map((action) => buildButton(action, statuses.toSet()))
          .toList(),
    );
  }
}
