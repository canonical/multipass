import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../delete_instance_dialog.dart';
import '../notifications.dart';
import '../providers.dart';
import '../vm_action.dart';

class VmActionButtons extends ConsumerWidget {
  final String name;

  const VmActionButtons(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);

    Function(VmAction) wrapInNotification(
      Future<void> Function(Iterable<String>) function,
    ) {
      return (action) {
        final notificationsNotifier = ref.read(notificationsProvider.notifier);
        notificationsNotifier.addOperation(
          function([name]),
          loading: '${action.continuousTense} $name',
          onSuccess: (_) => '${action.pastTense} $name',
          onError: (error) {
            return 'Failed to ${action.name.toLowerCase()} $name: $error';
          },
        );
      };
    }

    final actions = {
      VmAction.start: wrapInNotification(client.start),
      VmAction.stop: wrapInNotification(client.stop),
      VmAction.suspend: wrapInNotification(client.suspend),
      VmAction.delete: (action) {
        showDialog(
          context: context,
          barrierDismissible: false,
          builder: (_) => DeleteInstanceDialog(
            multiple: false,
            onDelete: () => wrapInNotification(client.purge)(action),
          ),
        );
      },
    };

    final actionButtons = [
      for (final MapEntry(key: action, value: function) in actions.entries)
        PopupMenuItem(
          padding: EdgeInsets.zero,
          enabled: false,
          child: ActionTile(name, action, () => function(action)),
        ),
    ];

    return PopupMenuButton(
      tooltip: 'Show actions',
      position: PopupMenuPosition.under,
      itemBuilder: (_) => actionButtons,
      child: Container(
        width: 110,
        height: 36,
        padding: const EdgeInsets.symmetric(horizontal: 8),
        decoration: BoxDecoration(
          border: Border.all(color: const Color(0xff333333)),
        ),
        child: const Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            Text('Actions', style: TextStyle(fontWeight: FontWeight.bold)),
            Icon(Icons.keyboard_arrow_down),
          ],
        ),
      ),
    );
  }
}

class ActionTile extends ConsumerWidget {
  final String name;
  final VmAction action;
  final VoidCallback function;

  const ActionTile(this.name, this.action, this.function, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final enabled = ref.watch(vmInfoProvider(name).select((info) {
      return action.allowedStatuses.contains(info.instanceStatus.status);
    }));

    return ListTile(
      enabled: enabled,
      contentPadding: const EdgeInsets.symmetric(horizontal: 16),
      title: Text(
        action.name,
        style: enabled ? const TextStyle(color: Colors.black) : null,
      ),
      onTap: () {
        Navigator.pop(context);
        function();
      },
    );
  }
}
