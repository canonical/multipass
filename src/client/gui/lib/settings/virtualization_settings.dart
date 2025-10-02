import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../riverpod_compat.dart';

import '../dropdown.dart';
import '../notifications/notifications_provider.dart';
import '../platform/platform.dart';
import '../providers.dart';

final driverProvider = daemonSettingProvider(driverKey);
final bridgedNetworkProvider = daemonSettingProvider(bridgedNetworkKey);

class VirtualizationSettings extends ConsumerWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final driver = ref.watch(driverProvider).valueOrNull;
    final bridgedNetwork = ref.watch(bridgedNetworkProvider).valueOrNull;
    final networksAsync = ref.watch(networksProvider);
    final networks = networksAsync.valueOrNull ?? const <String>{};

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          'Virtualization',
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 20),
        Dropdown(
          label: 'Driver',
          width: 260,
          value: driver,
          items: {if (driver != null) driver: driver, ...mpPlatform.drivers},
          onChanged: (value) {
            if (value == driver) return;
            ref
                .read(driverProvider.notifier)
                .set(value!)
                .onError(ref.notifyError((e) => 'Failed to set driver: $e'));
          },
        ),
        const SizedBox(height: 20),
        if (networks.isNotEmpty)
          Dropdown<String>(
            label: 'Bridged network',
            width: 260,
            value: networks.contains(bridgedNetwork) ? bridgedNetwork : '',
            items: {'': 'None', ...Map.fromIterable(networks)},
            onChanged: (value) {
              ref.read(bridgedNetworkProvider.notifier).set(value!).onError(
                    ref.notifyError((e) => 'Failed to set bridged network: $e'),
                  );
            },
          ),
      ],
    );
  }
}
