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

class ZonesDropdown extends ConsumerWidget {
  const ZonesDropdown({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    // Pre-fetch zones data
    final zonesAsync = ref.watch(zonesProvider);
    final vmInfos = ref.watch(vmInfosProvider);
    return PopupMenuButton(
      tooltip: 'Change zone availability',
      position: PopupMenuPosition.under,
      child: OutlinedButton(
        onPressed: null,
        child: const Text('Zones', style: TextStyle(color: Colors.black)),
      ),
      itemBuilder: (context) => zonesAsync.when(
          loading: () => [
                const PopupMenuItem(
                  enabled: false,
                  child: SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(),
                  ),
                ),
              ],
          error: (_, __) => [
                const PopupMenuItem(
                  enabled: false,
                  child: Text('Error loading zones'),
                ),
              ],
          data: (zones) => [
                const PopupMenuItem(
                  enabled: false,
                  child: Padding(
                    padding: EdgeInsets.symmetric(horizontal: 4, vertical: 8),
                    child: Text(
                      'Availability zones',
                      style: TextStyle(
                        fontSize: 24,
                        fontWeight: FontWeight.w600,
                        color: Colors.black,
                      ),
                    ),
                  ),
                ),
                ...zones.map((zone) {
                  // Count running instances in this zone
                  final runningCount = vmInfos
                      .where((vm) =>
                          vm.zone?.name == zone.name &&
                          vm.instanceStatus.status == Status.RUNNING)
                      .length;

                  return PopupMenuItem(
                    enabled: false,
                    padding: EdgeInsets.zero,
                    child: Container(
                      decoration: BoxDecoration(
                        border: Border.all(color: Colors.grey.withOpacity(0.3)),
                        borderRadius: BorderRadius.circular(4),
                      ),
                      margin: const EdgeInsets.symmetric(
                          horizontal: 8, vertical: 4),
                      padding: const EdgeInsets.all(8),
                      child: _ZoneToggleRow(
                        zone.name,
                        zone.available,
                        runningCount,
                      ),
                    ),
                  );
                }).toList(),
              ]),
    );
  }
}

class _ZoneToggleRow extends ConsumerWidget {
  final String zoneName;
  final bool available;
  final int instanceCount;

  const _ZoneToggleRow(this.zoneName, this.available, this.instanceCount);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);

    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('$zoneName',
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.w600,
                  color: Colors.black,
                )),
            Text(
              '$instanceCount running instance${instanceCount == 1 ? '' : 's'}',
              style: TextStyle(
                fontSize: 12,
                color: Colors.black,
              ),
            ),
          ],
        ),
        Switch(
          value: available,
          onChanged: (value) => client.zonesState([zoneName], value),
        ),
      ],
    );
  }
}

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
          loading: l10n.bulkActionMessage(action.continuousTense(l10n), object),
          onSuccess: (_) =>
              l10n.bulkActionMessage(action.pastTense(l10n), object),
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
            count: selectedVms.length,
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

    return SizedBox(
      height: 36,
      child: Row(
        children: [
          ...actionButtons.gap(width: 8).toList(),
          const SizedBox(width: 8),
          const ZonesDropdown(),
        ],
      ),
    );
  }
}
