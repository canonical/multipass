import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../providers.dart';

class ZoneDropdown extends ConsumerWidget {
  final String value;
  final ValueChanged<String?>? onChanged;
  final bool enabled;

  const ZoneDropdown({
    super.key,
    required this.value,
    required this.onChanged,
    this.enabled = true,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final zones = ref.watch(zonesProvider);
    final hasAvailableZones = zones.any((z) => z.available);

    // If selected zone is unavailable and there are no available zones, force auto selection
    if (!hasAvailableZones && value != 'auto') {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        onChanged?.call('auto');
      });
    }

    String? errorText;
    if (!hasAvailableZones) {
      errorText = 'All zones are unavailable';
    } else if (!zones.any((z) => z.name == value && z.available) &&
        value != 'auto') {
      errorText = '$value is unavailable';
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Text('Zone'),
            const SizedBox(width: 4),
            Tooltip(
              message:
                  'Zones simulate availability zones in public clouds\nand allow for testing resilience against real-world incidents',
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
          enabled: enabled,
          onChanged: onChanged,
          items: {
            'auto': 'Auto (assign automatically)',
            for (final zone in zones)
              zone.name:
                  '${zone.name}${zone.available ? '' : ' (unavailable)'}',
          },
          errorText: errorText,
        ),
      ],
    );
  }
}
