import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../extensions.dart';
import '../notifications.dart';
import '../providers.dart';
import '../vm_action.dart';
import 'vms.dart';

class BulkActionsBar extends ConsumerWidget {
  const BulkActionsBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);
    final selectedVms = ref.watch(selectedVmsProvider);
    final statuses = ref
        .watch(vmStatusesProvider)
        .asMap()
        .whereKey(selectedVms.contains)
        .values
        .toSet();

    final actions = [
      (VmAction.start, client.start),
      (VmAction.stop, client.stop),
      (VmAction.suspend, client.suspend),
      (VmAction.delete, client.delete),
    ];

    final actionButtons = [
      for (final (action, function) in actions)
        VmActionButton(
          action: action,
          currentStatuses: statuses,
          function: () {
            final object = selectedVms.length == 1
                ? selectedVms.first
                : '${selectedVms.length} instances';
            final notification = OperationNotification(
              text: '${action.continuousTense} $object',
              future: function(selectedVms).then((_) {
                return '${action.pastTense} $object';
              }).onError((_, __) {
                throw 'Failed to ${action.name.toLowerCase()} $object';
              }),
            );
            ref.read(notificationsProvider.notifier).add(notification);
          },
        ),
    ];

    return Row(children: actionButtons.gap(width: 8).toList());
  }
}
