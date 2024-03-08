import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../extensions.dart';
import '../notifications.dart';
import '../providers.dart';
import '../vm_action.dart';

class VmActionButtons extends ConsumerWidget {
  final String name;
  final Status status;

  const VmActionButtons({
    super.key,
    required this.name,
    required this.status,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);

    final actions = [
      (VmAction.start, client.start),
      (VmAction.stop, client.stop),
      (VmAction.suspend, client.suspend),
      (VmAction.delete, client.delete),
    ];

    final actionButtons = [
      VmActionButton(
        action: VmAction.edit,
        currentStatuses: {status},
        function: () => Scaffold.of(context).openEndDrawer(),
      ),
      for (final (action, function) in actions)
        VmActionButton(
          action: action,
          currentStatuses: {status},
          function: () {
            final notification = OperationNotification(
              text: '${action.continuousTense} $name',
              future: function([name]).then((_) {
                return '${action.pastTense} $name';
              }).onError((_, __) {
                throw 'Failed to ${action.name.toLowerCase()} $name';
              }),
            );
            ref.read(notificationsProvider.notifier).add(notification);
          },
        ),
    ];

    return Row(children: actionButtons.gap(width: 8).toList());
  }
}
