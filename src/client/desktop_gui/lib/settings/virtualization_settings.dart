import 'dart:io';

import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../providers.dart';

final networksProvider = Provider.autoDispose((ref) {
  ref.watch(daemonSettingProvider2(driverKey));
  if (ref.watch(daemonAvailableProvider)) {
    ref.watch(grpcClientProvider).networks().then((networks) {
      return ref.state = networks.map((n) => n.name).toBuiltSet();
    }).ignore();
  }
  return BuiltSet<String>();
});

final driverProvider = daemonSettingProvider2(driverKey);
final bridgedNetworkProvider = daemonSettingProvider2(bridgedNetworkKey);

class VirtualizationSettings extends ConsumerWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final driver = ref.watch(driverProvider).valueOrNull;
    final bridgedNetwork = ref.watch(bridgedNetworkProvider).valueOrNull;
    final networks = ref.watch(networksProvider);

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const SizedBox(height: 60),
      Dropdown(
        label: 'Driver',
        value: driver,
        items: drivers,
        onChanged: (value) {
          ref.read(driverProvider.notifier).set(value!);
        },
      ),
      const SizedBox(height: 48),
      if (networks.isNotEmpty)
        Dropdown(
          label: 'Virtual interface',
          value: networks.contains(bridgedNetwork) ? bridgedNetwork : null,
          items: Map.fromIterable(networks),
          onChanged: (value) {
            ref.read(bridgedNetworkProvider.notifier).set(value!);
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
