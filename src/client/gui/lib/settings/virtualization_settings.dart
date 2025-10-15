import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

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
                .set(value as String)
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
