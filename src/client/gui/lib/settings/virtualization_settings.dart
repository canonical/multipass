import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:rxdart/rxdart.dart';

import '../dropdown.dart';
import '../l10n/app_localizations.dart';
import '../notifications/driver_migration_notification.dart';
import '../notifications/notifications_provider.dart';
import '../platform/platform.dart';
import '../providers.dart';
import 'constants.dart';

final driverProvider = daemonSettingProvider(driverKey);
final bridgedNetworkProvider = daemonSettingProvider(bridgedNetworkKey);

// TODO hyperv migration, remove
class MigrationInProgressNotifier extends Notifier<bool> {
  @override
  bool build() {
    return false;
  }

  void set(bool value) {
    state = value;
  }
}

// TODO hyperv migration, remove
final migrationInProgressProvider =
    NotifierProvider<MigrationInProgressNotifier, bool>(
  MigrationInProgressNotifier.new,
);

// TODO hyperv migration, remove
// Switching from hyperv migrates the instances, which is reported as the change
// progresses. The driver can't be changed again until the migration is done.
void migrateToHcs(WidgetRef ref) {
  final inProgress = ref.read(migrationInProgressProvider.notifier);
  inProgress.set(true);
  final replies = ref.read(driverProvider.notifier).setStreaming('hcs');
  ref.read(notificationsProvider.notifier).add(
        DriverMigrationNotification(
          progress:
              migrationProgress(replies).doOnDone(() => inProgress.set(false)),
        ),
      );
}

class VirtualizationSettings extends ConsumerWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final driver = ref.watch(driverProvider).when(
          data: (data) => data,
          loading: () => null,
          error: (_, __) => null,
        );
    final bridgedNetwork = ref.watch(bridgedNetworkProvider).when(
          data: (data) => data,
          loading: () => null,
          error: (_, __) => null,
        );
    final networks = ref.watch(networksProvider).when(
          data: (data) => data,
          loading: () => const <String>{},
          error: (_, __) => const <String>{},
        );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          l10n.virtualizationTitle,
          style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 20),
        Dropdown(
          label: l10n.virtualizationDriverLabel,
          width: settingFieldWidth,
          value: driver,
          items: {if (driver != null) driver: driver, ...mpPlatform.drivers},
          // TODO hyperv migration, remove
          enabled: !ref.watch(migrationInProgressProvider),
          onChanged: (value) {
            if (value == driver) return;
            // TODO hyperv migration, remove
            if (driver == 'hyperv' && value == 'hcs') {
              migrateToHcs(ref);
              return;
            }
            ref.read(driverProvider.notifier).set(value as String).onError(
                ref.notifyError((e) => l10n.virtualizationDriverError('$e')));
          },
        ),
        const SizedBox(height: 20),
        if (networks.isNotEmpty)
          Dropdown<String>(
            label: l10n.bridgeTitle,
            width: settingFieldWidth,
            value: networks.contains(bridgedNetwork) ? bridgedNetwork : '',
            items: {
              '': l10n.virtualizationBridgedNetworkNone,
              ...Map.fromIterable(networks)
            },
            onChanged: (value) {
              ref.read(bridgedNetworkProvider.notifier).set(value!).onError(
                    ref.notifyError((e) => l10n.bridgeFailedNetwork('$e')),
                  );
            },
          ),
      ],
    );
  }
}
