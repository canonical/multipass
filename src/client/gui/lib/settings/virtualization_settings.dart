import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../notifications/notifications_provider.dart';
import '../providers.dart';

final driverProvider = daemonSettingProvider(driverKey);
final bridgedNetworkProvider = daemonSettingProvider(bridgedNetworkKey);

class VirtualizationSettings extends ConsumerWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final driver = ref.watch(driverProvider).valueOrNull;
    final bridgedNetwork = ref.watch(bridgedNetworkProvider).valueOrNull;
    final networks = ref.watch(networksProvider);

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const Text(
        'Virtualization',
        style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
      ),
      const SizedBox(height: 20),
      Dropdown(
        label: 'Driver',
        width: 260,
        value: driver,
        items: drivers,
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
        Dropdown(
          label: 'Virtual interface',
          width: 260,
          value: networks.contains(bridgedNetwork) ? bridgedNetwork : null,
          items: Map.fromIterable(networks),
          onChanged: (value) {
            ref.read(bridgedNetworkProvider.notifier).set(value!).onError(
                ref.notifyError((e) => 'Failed to set virtual interface: $e'));
          },
        ),
    ]);
  }
}

final drivers = () {
  if (Platform.isLinux) {
    return const {'qemu': 'Qemu', 'lxd': 'LXD', 'libvirt': 'Libvirt'};
  }
  if (Platform.isMacOS) {
    return const {'qemu': 'Qemu', 'virtualbox': 'VirtualBox'};
  }
  if (Platform.isWindows) {
    return const {'hyperv': 'Hyper-V', 'virtualbox': 'VirtualBox'};
  }
  throw const OSError('Unsupported OS');
}();
