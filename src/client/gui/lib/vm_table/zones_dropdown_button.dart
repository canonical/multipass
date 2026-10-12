import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_svg/flutter_svg.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../colors.dart';
import '../confirmation_dialog.dart';
import '../l10n/app_localizations.dart';
import '../providers.dart';
import '../switch.dart';

class ZonesDropdownButton extends ConsumerWidget {
  const ZonesDropdownButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final zones = ref.watch(zonesProvider);
    if (!ref.watch(azSupportedProvider)) return const SizedBox.shrink();
    final unavailableZones = zones.where((z) => !z.available).length;

    return Row(
      children: [
        PopupMenuButton(
          tooltip: l10n.vmTableZonesButtonTooltip,
          position: PopupMenuPosition.under,
          constraints: const BoxConstraints(
            minWidth: 400, // Set minimum width for the popup menu
            maxWidth: 500,
          ),
          itemBuilder: (context) => [
            PopupMenuItem(
              enabled: false,
              child: Container(
                width: double.infinity, // Take full width of the popup
                padding: const EdgeInsets.only(
                    left: 8, right: 16, top: 16, bottom: 12),
                child: Text(
                  l10n.vmTableZonesPopupTitle,
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
                padding: const EdgeInsets.only(left: 24, right: 12),
                child: Container(
                  width: double.infinity, // Take full width of the popup
                  decoration: BoxDecoration(
                    border: zone == zones.last
                        ? Border()
                        : Border(
                            bottom:
                                BorderSide(color: Colors.grey.withAlpha(77))),
                  ),
                  margin: const EdgeInsets.symmetric(vertical: 4),
                  padding: const EdgeInsets.only(top: 8, bottom: 16),
                  child: _ZoneToggleRow(zone.name),
                ),
              ),
          ],
          child: OutlinedButton(
            onPressed: null,
            style: OutlinedButton.styleFrom(
              padding:
                  const EdgeInsets.only(left: 8, right: 4, top: 16, bottom: 16),
              minimumSize: Size.zero,
              tapTargetSize: MaterialTapTargetSize.shrinkWrap,
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                SvgPicture.asset('assets/zones.svg'),
                const SizedBox(width: 8),
                Text(l10n.vmTableZonesButtonTitle,
                    style: TextStyle(color: Colors.black)),
                const SizedBox(width: 4),
                const Icon(Icons.expand_more, color: Colors.black, size: 20),
              ],
            ),
          ),
        ),
        if (unavailableZones > 0) ...[
          const SizedBox(width: 8),
          Icon(Icons.warning_rounded, color: warningAmber, size: 24),
          const SizedBox(width: 4),
          Text(
            l10n.vmTableZonesUnavailableLabel(unavailableZones, zones.length),
            style: TextStyle(color: Colors.black),
          ),
        ],
      ],
    );
  }
}

class _ZoneToggleRow extends ConsumerWidget {
  final String zoneName;

  const _ZoneToggleRow(this.zoneName);

  Future<void> toggleZoneAvailability(
      BuildContext context, WidgetRef ref, bool value) async {
    if (!value) {
      final unstoppedInstancesCount = ref.read(vmInfosProvider.select((infos) {
        return infos
            .where((vm) => vm.zone.name == zoneName)
            .where((vm) =>
                vm.instanceStatus.status == Status.RUNNING ||
                vm.instanceStatus.status == Status.STARTING ||
                vm.instanceStatus.status == Status.RESTARTING ||
                vm.instanceStatus.status == Status.DELAYED_SHUTDOWN ||
                vm.instanceStatus.status == Status.SUSPENDING ||
                vm.instanceStatus.status == Status.SUSPENDED)
            .length;
      }));

      if (unstoppedInstancesCount > 0) {
        final confirmed = await showDialog<bool>(
          context: context,
          barrierDismissible: false,
          builder: (context) {
            final l10n = AppLocalizations.of(context)!;
            return ConfirmationDialog(
              title: l10n.vmTableZonesMakeUnavailablePopupTitle(zoneName),
              body: Padding(
                padding: const EdgeInsets.only(left: 8),
                child: Text(l10n.vmTableZonesMakeUnavailablePopupContent(
                    unstoppedInstancesCount, zoneName)),
              ),
              actionText: l10n.vmTableZonesMakeUnavailablePopupAction(
                  unstoppedInstancesCount),
              onAction: () => Navigator.pop(context, true),
              inactionText: l10n.commonCancel,
              onInaction: () => Navigator.pop(context, false),
            );
          },
        );

        if (confirmed != true) {
          return;
        }
      }
    }

    final client = ref.read(grpcClientProvider);
    client.zonesState([zoneName], value);
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final client = ref.watch(grpcClientProvider);

    final available = ref.watch(zonesProvider.select((zones) {
      return zones.firstWhere((z) => z.name == zoneName).available;
    }));

    final runningInstancesCount = ref.watch(vmInfosProvider.select((infos) {
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
                onChanged: (value) =>
                    toggleZoneAvailability(context, ref, value),
                size: 28,
              ),
            ),
          ],
        ),
        Text(
          l10n.vmTableZonesRunningInstanceCount(runningInstancesCount),
          style: TextStyle(fontSize: 16, height: 1.5, color: Colors.black),
        ),
      ],
    );
  }
}
