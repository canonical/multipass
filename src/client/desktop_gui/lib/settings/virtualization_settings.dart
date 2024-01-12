import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../providers.dart';

final driverProvider = FutureProvider(
  (ref) => ref.watch(grpcClientProvider).get('local.driver'),
);

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
          onChanged: (_) {},
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
