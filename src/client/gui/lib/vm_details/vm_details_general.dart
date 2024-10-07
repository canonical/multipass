import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';

import '../extensions.dart';
import '../providers.dart';
import 'cpu_sparkline.dart';
import 'memory_usage.dart';
import 'vm_action_buttons.dart';
import 'vm_details.dart';
import 'vm_status_icon.dart';

class VmDetailsHeader extends ConsumerWidget {
  final String name;

  const VmDetailsHeader(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(vmInfoProvider(name));

    final cpu = VmStat(
      width: 120,
      height: 35,
      label: 'CPU USAGE',
      child: CpuSparkline(info.name),
    );

    final memory = VmStat(
      width: 110,
      height: 35,
      label: 'MEMORY USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.memoryUsage,
        total: info.memoryTotal,
      ),
    );

    final disk = VmStat(
      width: 110,
      height: 35,
      label: 'DISK USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.diskUsage,
        total: info.diskTotal,
      ),
    );

    final currentLocation = ref.watch(vmScreenLocationProvider(name));
    final buttonStyle = Theme.of(context).outlinedButtonTheme.style;

    OutlinedButton locationButton(VmDetailsLocation location) {
      final style = buttonStyle?.copyWith(
        shape: const MaterialStatePropertyAll(RoundedRectangleBorder()),
        backgroundColor: location == currentLocation
            ? const MaterialStatePropertyAll(Color(0xff333333))
            : null,
        foregroundColor: location == currentLocation
            ? const MaterialStatePropertyAll(Colors.white)
            : null,
      );
      return OutlinedButton(
        style: style,
        child: Text(location.name.capitalize()),
        onPressed: () {
          ref.read(vmScreenLocationProvider(name).notifier).state = location;
        },
      );
    }

    final locationButtons = Row(mainAxisSize: MainAxisSize.min, children: [
      locationButton(VmDetailsLocation.shells),
      locationButton(VmDetailsLocation.details),
    ]);

    final list = [
      Expanded(
        child: Text(
          name.nonBreaking,
          style: const TextStyle(fontSize: 24, fontWeight: FontWeight.w300),
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
        ),
      ),
      locationButtons,
      cpu,
      memory,
      disk,
      VmActionButtons(name),
    ];

    return Padding(
      padding: const EdgeInsets.all(20),
      child: Row(children: list.gap(width: 40).toList()),
    );
  }
}

class VmStat extends StatelessWidget {
  final String label;
  final Widget child;
  final double width;
  final double height;

  const VmStat({
    super.key,
    required this.label,
    required this.child,
    required this.width,
    required this.height,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: width,
      height: height,
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(
          label,
          style: const TextStyle(fontSize: 10, fontWeight: FontWeight.bold),
        ),
        Expanded(child: Align(alignment: Alignment.centerLeft, child: child)),
      ]),
    );
  }
}

class GeneralDetails extends ConsumerWidget {
  final String name;

  const GeneralDetails(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(vmInfoProvider(name));

    final status = VmStat(
      width: 100,
      height: 50,
      label: 'STATE',
      child: VmStatusIcon(info.instanceStatus.status),
    );

    final image = VmStat(
      width: 150,
      height: 50,
      label: 'IMAGE',
      child: Text(info.instanceInfo.currentRelease),
    );

    final privateIp = VmStat(
      width: 150,
      height: 50,
      label: 'PRIVATE IP',
      child: Text(info.instanceInfo.ipv4.firstOrNull ?? '-'),
    );

    final publicIp = VmStat(
      width: 150,
      height: 50,
      label: 'PUBLIC IP',
      child: Text(info.instanceInfo.ipv4.skip(1).firstOrNull ?? '-'),
    );

    final created = VmStat(
      width: 140,
      height: 50,
      label: 'CREATED',
      child: Text(
        DateFormat('yyyy-MM-dd HH:mm:ss')
            .format(info.instanceInfo.creationTimestamp.toDateTime()),
      ),
    );

    final uptime = VmStat(
      width: 300,
      height: 50,
      label: 'UPTIME',
      child: Text(info.instanceInfo.uptime),
    );

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(
          height: 50,
          child: Text('General', style: TextStyle(fontSize: 24)),
        ),
        Wrap(spacing: 50, runSpacing: 25, children: [
          status,
          image,
          privateIp,
          publicIp,
          created,
          uptime,
        ]),
      ],
    );
  }
}
