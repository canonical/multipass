import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';

import '../providers.dart';
import 'cpu_sparkline.dart';
import 'memory_usage.dart';
import 'vm_status_icon.dart';

class VmDetailsScreen extends ConsumerWidget {
  final String name;

  const VmDetailsScreen({
    super.key,
    required this.name,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(vmInfosProvider.select((infos) {
      return infos.firstWhereOrNull((info) => info.name == name) ??
          DetailedInfoItem();
    }));

    final vmName = Text(name, style: const TextStyle(fontSize: 37));

    final image = VmStat(
      label: 'IMAGE',
      child: Text(info.instanceInfo.currentRelease),
    );

    final status = VmStat(
      label: 'STATE',
      child: VmStatusIcon(info.instanceStatus.status),
    );

    final cpu = VmStat(
      label: 'CPU USAGE',
      child: SizedBox(
        height: 40,
        width: 150,
        child: CpuSparkline(info.name),
      ),
    );

    final memory = VmStat(
      label: 'MEMORY USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.memoryUsage,
        total: info.memoryTotal,
      ),
    );

    final disk = VmStat(
      label: 'DISK USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.diskUsage,
        total: info.diskTotal,
      ),
    );

    final privateIp = VmStat(
      label: 'PRIVATE IP',
      child: Text(info.instanceInfo.ipv4.firstOrNull ?? '-'),
    );

    final publicIp = VmStat(
      label: 'PUBLIC IP',
      child: Text(info.instanceInfo.ipv4.skip(1).firstOrNull ?? '-'),
    );

    final mounts = VmStat(
      label: 'MOUNTS',
      child: Text(
        info.mountInfo.mountPaths
            .map((m) => '${m.sourcePath} → ${m.targetPath}')
            .join('\n'),
      ),
    );

    final created = VmStat(
      label: 'CREATED',
      child: Text(
        DateFormat('yyyy-MM-dd\nHH:mm:ss')
            .format(info.instanceInfo.creationTimestamp.toDateTime()),
      ),
    );

    final uptime = VmStat(
      label: 'UPTIME',
      child: Text(info.instanceInfo.uptime),
    );

    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.only(left: 16, right: 16, top: 75),
        child: Column(children: [
          vmName,
          image,
          status,
          cpu,
          memory,
          disk,
          privateIp,
          publicIp,
          mounts,
          created,
          uptime,
        ]),
      ),
    );
  }
}

class VmStat extends StatelessWidget {
  final String label;
  final Widget child;

  const VmStat({
    super.key,
    required this.label,
    required this.child,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          label,
          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.bold),
        ),
        child,
      ],
    );
  }
}
