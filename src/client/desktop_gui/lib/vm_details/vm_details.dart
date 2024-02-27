import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';

import '../extensions.dart';
import '../providers.dart';
import 'cpu_sparkline.dart';
import 'edit_vm_form.dart';
import 'memory_usage.dart';
import 'terminal_tabs.dart';
import 'vm_action_buttons.dart';
import 'vm_status_icon.dart';

class VmDetailsScreen extends StatelessWidget {
  final String name;

  const VmDetailsScreen({super.key, required this.name});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      endDrawer: EditVmForm(name),
      body: Padding(
        padding: const EdgeInsets.all(20).copyWith(top: 75),
        child: Column(children: [
          VmDetails(name: name),
          Expanded(child: TerminalTabs(name)),
        ]),
      ),
    );
  }
}

class VmDetails extends ConsumerWidget {
  final String name;

  const VmDetails({super.key, required this.name});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(vmInfoProvider(name));

    final image = VmStat(
      width: 150,
      label: 'IMAGE',
      child: Text(info.instanceInfo.currentRelease),
    );

    final status = VmStat(
      width: 100,
      label: 'STATE',
      child: VmStatusIcon(info.instanceStatus.status),
    );

    final cpu = VmStat(
      width: 150,
      label: 'CPU USAGE',
      child: CpuSparkline(info.name),
    );

    final memory = VmStat(
      width: 150,
      label: 'MEMORY USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.memoryUsage,
        total: info.memoryTotal,
      ),
    );

    final disk = VmStat(
      width: 150,
      label: 'DISK USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.diskUsage,
        total: info.diskTotal,
      ),
    );

    final privateIp = VmStat(
      width: 150,
      label: 'PRIVATE IP',
      child: Text(info.instanceInfo.ipv4.firstOrNull ?? '-'),
    );

    final publicIp = VmStat(
      width: 150,
      label: 'PUBLIC IP',
      child: Text(info.instanceInfo.ipv4.skip(1).firstOrNull ?? '-'),
    );

    final mounts = VmStat(
      width: 150,
      label: 'MOUNTS',
      child: Text(
        info.mountInfo.mountPaths
            .map((m) => '${m.sourcePath} → ${m.targetPath}')
            .join('\n'),
      ),
    );

    final created = VmStat(
      width: 100,
      label: 'CREATED',
      child: Text(
        DateFormat('yyyy-MM-dd\nHH:mm:ss')
            .format(info.instanceInfo.creationTimestamp.toDateTime()),
      ),
    );

    final uptime = VmStat(
      width: 300,
      label: 'UPTIME',
      child: Text(info.instanceInfo.uptime),
    );

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Row(children: [
        Expanded(
          child: Text(
            name.nonBreaking,
            style: const TextStyle(fontSize: 37),
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        VmActionButtons(name: name, status: info.instanceStatus.status),
      ]),
      const SizedBox(height: 30),
      Wrap(spacing: 20, runSpacing: 20, children: [
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
    ]);
  }
}

class VmStat extends StatelessWidget {
  final String label;
  final Widget child;
  final double width;

  const VmStat({
    super.key,
    required this.label,
    required this.child,
    required this.width,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: width,
      height: 75,
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(
          label,
          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.bold),
        ),
        Expanded(child: Align(alignment: Alignment.centerLeft, child: child)),
      ]),
    );
  }
}
