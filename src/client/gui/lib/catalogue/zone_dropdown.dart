import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../providers.dart';

class ZoneDropdown extends ConsumerWidget {
  final String value;
  final ValueChanged<String?> onChanged;

  const ZoneDropdown({
    super.key,
    required this.value,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final zones = ref.watch(zonesProvider);
    final availableZones = zones.where((z) => z.available).toList();
    final hasAvailableZones = availableZones.isNotEmpty;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Text('Pseudo zone'),
            const SizedBox(width: 4),
            Tooltip(
              message:
                  'Pseudo zones simulate availability zones in public clouds\nand allow for testing resilience against real-world incidents',
              child: Icon(
                Icons.info_outline,
                size: 16,
                color: Colors.blue[700],
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        Dropdown<String>(
          value: value,
          onChanged: onChanged,
          items: {
            'auto': 'Auto (assign automatically)',
            for (final zone in availableZones) zone.name: zone.name,
          },
          errorText: hasAvailableZones ? null : 'All zones are unavailable',
        ),
      ],
    );
  }
}
