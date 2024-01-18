import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../providers.dart';

const driverKey = 'local.driver';

final driverProvider = FutureProvider((ref) {
  ref.watch(daemonAvailableProvider);
  return ref.watch(grpcClientProvider).get(driverKey);
});


class VirtualizationSettings extends ConsumerWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final driver = ref.watch(driverProvider).valueOrNull;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(height: 60),
        Dropdown(
          label: 'Driver',
          value: driver,
          items: drivers,
          onChanged: (value) => ref
              .read(grpcClientProvider)
              .set(driverKey, value!)
              .onError((_, __) {}),
        ),
      ],
    );
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
