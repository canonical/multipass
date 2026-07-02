import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../l10n/app_localizations.dart';
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
    final l10n = AppLocalizations.of(context)!;
    final zones = ref.watch(zonesProvider);
    final hasAvailableZones = zones.any((z) => z.available);

    // Determine the default zone based on availability
    String defaultZone = '';
    for (final zone in zones) {
      if (zone.available) {
        defaultZone = zone.name;
        break;
      }
    }

    // If no zone is selected and we have a default, set it
    if (value.isEmpty && defaultZone.isNotEmpty) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        onChanged?.call(defaultZone);
      });
    }

    String? errorText;
    if (!hasAvailableZones) {
      errorText = l10n.zoneDropdownAllZonesUnavailableError;
    } else if (!zones.any((z) => z.name == value && z.available)) {
      errorText = l10n.zoneDropdownSelectedZoneUnavailableError(value);
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Text(l10n.zoneDropdownTitle),
            const SizedBox(width: 4),
            Tooltip(
              message: l10n.zoneDropdownTooltip,
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
          value: value.isNotEmpty ? value : defaultZone,
          enabled: enabled,
          onChanged: onChanged,
          items: {
            for (final zone in zones)
              zone.name: zone.available
                  ? zone.name
                  : l10n.zoneDropdownUnavailableZone(zone.name),
          },
          errorText: errorText,
        ),
      ],
    );
  }
}
