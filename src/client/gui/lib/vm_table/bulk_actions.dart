import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../delete_instance_dialog.dart';
import '../extensions.dart';
import '../l10n/app_localizations.dart';
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

    final l10n = AppLocalizations.of(context)!;

    Function(VmAction) wrapInNotification(
      Future<void> Function(Iterable<String>) function,
    ) {
      return (action) {
        final object = selectedVms.length == 1
            ? selectedVms.first
            : l10n.bulkActionInstanceCount(selectedVms.length);

        final notificationsNotifier = ref.read(notificationsProvider.notifier);
        notificationsNotifier.addOperation(
          function(selectedVms),
          loading: l10n.bulkActionLoading(action.continuousTense(l10n), object),
          onSuccess: (_) =>
              l10n.bulkActionSuccess(action.pastTense(l10n), object),
          onError: (error) {
            return l10n.bulkActionError(
                action.label(l10n).toLowerCase(), object, '$error');
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
            multiple: selectedVms.length > 1,
            onDelete: () => wrapInNotification(client.purge)(action),
          ),
        );
      },
    };

    final actionButtons = [
      for (final MapEntry(key: action, value: function) in actions.entries)
        VmActionButton(
          action: action,
          currentStatuses: statuses,
          function: () => function(action),
        ),
    ];

    return Row(children: actionButtons.gap(width: 8).toList());
  }
}
