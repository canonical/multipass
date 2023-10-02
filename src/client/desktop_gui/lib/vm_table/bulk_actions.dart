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
    final active = action.allowedStatuses.intersection(statuses).isNotEmpty;
    final isDelete = action.name == 'Delete';
    final textColor = isDelete ? Colors.white : Colors.black;

    final style = TextButton.styleFrom(
      backgroundColor: isDelete ? const Color(0xffC7162B) : Colors.white,
      foregroundColor: textColor,
      disabledForegroundColor: textColor.withOpacity(0.5),
      padding: const EdgeInsets.all(16),
      textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.w300),
      shape: const RoundedRectangleBorder(borderRadius: BorderRadius.zero),
    ).copyWith(side: MaterialStateBorderSide.resolveWith((states) {
      final disabled = states.contains(MaterialState.disabled);
      return isDelete
          ? BorderSide.none
          : BorderSide(
              color: const Color(0xff333333).withOpacity(disabled ? 0.5 : 1),
              width: 1.2,
            );
    }));

    return Container(
      margin: const EdgeInsets.only(right: 8),
      child: TextButton(
        onPressed: active ? () => action.function(action.vmNames) : null,
        style: style,
        child: Text(action.name),
      ),
    );
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
