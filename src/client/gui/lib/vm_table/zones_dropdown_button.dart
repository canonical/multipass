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
      constraints: const BoxConstraints(
        minWidth: 400, // Set minimum width for the popup menu
        maxWidth: 500, // Set maximum width to prevent it from being too wide
      ),
      child: OutlinedButton(
        onPressed: null,
        style: OutlinedButton.styleFrom(
          padding:
              const EdgeInsets.only(left: 8, right: 4, top: 15, bottom: 15),
          minimumSize: Size.zero,
          tapTargetSize: MaterialTapTargetSize.shrinkWrap,
        ),
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
        PopupMenuItem(
          enabled: false,
          child: Container(
            width: double.infinity, // Take full width of the popup
            padding:
                const EdgeInsets.only(left: 8, right: 16, top: 16, bottom: 12),
            child: const Text(
              'Enable/disable zones',
              style: TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.w600,
                color: Colors.black,
              ),
              maxLines: 1, // Prevent text wrapping
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ),
        for (final zone in zones)
          PopupMenuItem(
            enabled: false,
            padding:  const EdgeInsets.only(left: 24.0, right: 12.0),
            child: Container(
              width: double.infinity, // Take full width of the popup
              decoration: BoxDecoration(
                border: zone == zones.last ? Border() : Border(bottom: BorderSide(color: Colors.grey.withAlpha(77))),
              ),
              margin: const EdgeInsets.symmetric(vertical: 4),
              padding:
                  const EdgeInsets.only(top: 8.0, bottom: 16.0),
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

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(
              zoneName,
              style: TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.w600,
                color: Colors.black,
              ),
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
        ),
        Text(
          '${instanceCount == 0 ? 'no' : instanceCount} running instance${instanceCount == 1 ? '' : 's'}',
          style: TextStyle(fontSize: 16, height: 1.5, color: Colors.black),
        ),
      ],
    );
  }
}
