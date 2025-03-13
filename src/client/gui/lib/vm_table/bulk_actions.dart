import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../delete_instance_dialog.dart';
import '../extensions.dart';
import '../notifications.dart';
import '../providers.dart';
import '../vm_action.dart';
import 'vms.dart';

class ZonesDropdown extends ConsumerWidget {
  const ZonesDropdown({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return PopupMenuButton(
      offset: const Offset(0, 40),
      child: OutlinedButton(
        onPressed: null,
        child: const Text('Zones', style: TextStyle(color: Colors.black)),
      ),
      itemBuilder: (context) {
        final zonesAsync = ref.watch(zonesProvider);
        return [
          PopupMenuItem(
            enabled: false,
            child: zonesAsync.when(
              loading: () => const SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(),
              ),
              error: (_, __) => const Text('Error loading zones'),
              data: (zones) => Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  for (final zone in zones) ...[
                    _ZoneToggleRow(zone.name),
                    if (zone != zones.last) const SizedBox(height: 8),
                  ],
                ],
              ),
            ),
          ),
        ];
      },
    );
  }
}

class _ZoneToggleRow extends ConsumerWidget {
  final String zoneName;

  const _ZoneToggleRow(this.zoneName);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final available = ref.watch(zoneStatusProvider(zoneName));
    final client = ref.watch(grpcClientProvider);

    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text('Zone $zoneName'),
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

    Function(VmAction) wrapInNotification(
      Future<void> Function(Iterable<String>) function,
    ) {
      return (action) {
        final object = selectedVms.length == 1
            ? selectedVms.first
            : '${selectedVms.length} instances';

        final notificationsNotifier = ref.read(notificationsProvider.notifier);
        notificationsNotifier.addOperation(
          function(selectedVms),
          loading: '${action.continuousTense} $object',
          onSuccess: (_) => '${action.pastTense} $object',
          onError: (error) {
            return 'Failed to ${action.name.toLowerCase()} $object: $error';
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

    return Container(
      height: 36,
      margin: const EdgeInsets.only(top: 20),
      child: Row(
        children: [
          Expanded(
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: Row(
                children: [
                  ...actionButtons.gap(width: 8).toList(),
                  const SizedBox(width: 8),
                  const ZonesDropdown(),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
