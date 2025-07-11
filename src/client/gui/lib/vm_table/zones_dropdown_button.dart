import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_svg/flutter_svg.dart';
import '../switch.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';

class ZonesDropdownButton extends ConsumerWidget {
  const ZonesDropdownButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final zones = ref.watch(zonesProvider);

    return PopupMenuButton(
      tooltip: 'Change zone availability',
      position: PopupMenuPosition.under,
      child: OutlinedButton(
        onPressed: null,
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            SvgPicture.asset('assets/zones.svg'),
            const SizedBox(width: 8),
            const Text('Zones', style: TextStyle(color: Colors.black)),
            const SizedBox(width: 4),
            const Icon(Icons.expand_more, color: Colors.black, size: 20),
          ],
        ),
      ),
      itemBuilder: (context) => [
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
        for (final zone in zones)
          PopupMenuItem(
            enabled: false,
            padding: EdgeInsets.zero,
            child: Container(
              decoration: BoxDecoration(
                border: Border.all(color: Colors.grey.withAlpha(77)),
                borderRadius: BorderRadius.circular(4),
              ),
              margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
              padding: const EdgeInsets.all(8),
              child: _ZoneToggleRow(zone.name),
            ),
          ),
      ],
    );
  }
}

class _ZoneToggleRow extends ConsumerWidget {
  final String zoneName;

  const _ZoneToggleRow(this.zoneName);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);

    final available = ref.watch(zonesProvider.select((zones) {
      return zones.firstWhere((z) => z.name == zoneName).available;
    }));

    final instanceCount = ref.watch(vmInfosProvider.select((infos) {
      return infos
          .where((vm) => vm.zone.name == zoneName)
          .where((vm) => vm.instanceStatus.status == Status.RUNNING)
          .length;
    }));

    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              zoneName,
              style: TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.w600,
                color: Colors.black,
              ),
            ),
            Text(
              '$instanceCount running instance${instanceCount == 1 ? '' : 's'}',
              style: TextStyle(fontSize: 12, color: Colors.black),
            ),
          ],
        ),
        MouseRegion(
          cursor: SystemMouseCursors.click,
          child: Switch(
            value: available,
            onChanged: (value) => client.zonesState([zoneName], value),
            size: 20,
          ),
        ),
      ],
    );
  }
}
