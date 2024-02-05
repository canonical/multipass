import 'dart:collection';

import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';

import '../providers.dart';
import 'memory_usage.dart';

extension on Iterable<String> {
  int get sum => map((value) => int.tryParse(value) ?? 0).sum;
}

List<Widget> totalUsageRow(
  Iterable<VmInfo> infos,
  Map<String, bool> enabledHeaders,
) {
  final usedMemory = infos.map((i) => i.instanceInfo.memoryUsage).sum;
  final totalMemory = infos.map((i) => i.memoryTotal).sum;
  final usedDisk = infos.map((i) => i.instanceInfo.diskUsage).sum;
  final totalDisk = infos.map((i) => i.diskTotal).sum;

  return [
    const SizedBox.shrink(),
    Container(
      margin: const EdgeInsets.all(10),
      alignment: Alignment.centerLeft,
      child: const Text(
        "Total",
        style: TextStyle(fontWeight: FontWeight.bold),
      ),
    ),
    for (final MapEntry(key: name, value: enabled)
        in enabledHeaders.entries.skip(2))
      if (name == 'MEMORY USAGE' && enabled)
        Container(
          margin: const EdgeInsets.all(10),
          child: MemoryUsage(used: '$usedMemory', total: '$totalMemory'),
        )
      else if (name == 'DISK USAGE' && enabled)
        Container(
          margin: const EdgeInsets.all(10),
          child: MemoryUsage(used: '$usedDisk', total: '$totalDisk'),
        )
      else if (enabled)
        const SizedBox.shrink(),
  ];
}

Widget headerBuilder(String name) {
  return Container(
    alignment: Alignment.centerLeft,
    margin: const EdgeInsets.only(left: 10),
    child: Text(name,
        style: const TextStyle(
          fontWeight: FontWeight.bold,
        )),
  );
}

Widget ellipsizedText(String text) {
  return Tooltip(
    message: text,
    child: Text(
      text.replaceAll('-', '\u2011').replaceAll(' ', '\u00A0'),
      overflow: TextOverflow.ellipsis,
    ),
  );
}

Widget vmStatus(Status status) {
  const icons = {
    Status.RUNNING: Icon(Icons.circle, color: Color(0xff0C8420), size: 10),
    Status.STOPPED: Icon(Icons.circle, color: Colors.black, size: 10),
    Status.SUSPENDED: Icon(Icons.circle, color: Color(0xff666666), size: 10),
    Status.RESTARTING:
        Icon(Icons.more_horiz, color: Color(0xff757575), size: 15),
    Status.STARTING: Icon(Icons.more_horiz, color: Color(0xff757575), size: 15),
    Status.SUSPENDING:
        Icon(Icons.more_horiz, color: Color(0xff757575), size: 15),
    Status.DELETED: Icon(Icons.close, color: Colors.redAccent, size: 15),
    Status.DELAYED_SHUTDOWN:
        Icon(Icons.circle, color: Color(0xff0C8420), size: 10),
  };

  final statusName = status.name.toLowerCase().replaceAll('_', ' ');
  return Row(children: [
    icons[status] ?? const Icon(Icons.help, color: Color(0xff757575), size: 15),
    const SizedBox(width: 8),
    Expanded(child: ellipsizedText(statusName)),
  ]);
}

Widget ipAddresses(Iterable<String> ips) {
  final firstIp = ips.firstOrNull ?? '-';
  final restIps = ips.skip(1).toList();

  return Row(children: [
    Expanded(child: ellipsizedText(firstIp)),
    if (restIps.isNotEmpty)
      Badge.count(
        count: restIps.length,
        alignment: const AlignmentDirectional(-1.5, 0.4),
        textStyle: const TextStyle(fontSize: 10),
        largeSize: 14,
        child: PopupMenuButton<void>(
          icon: const Icon(Icons.keyboard_arrow_down),
          position: PopupMenuPosition.under,
          tooltip: 'Other IP addresses',
          splashRadius: 10,
          itemBuilder: (_) => [
            const PopupMenuItem(
              enabled: false,
              child: Text('Other IP addresses'),
            ),
            ...restIps.map((ip) => PopupMenuItem(child: Text(ip))),
          ],
        ),
      ),
  ]);
}

class CpuUsagesNotifier
    extends AutoDisposeNotifier<Map<String, Queue<double>>> {
  @override
  Map<String, Queue<double>> build() {
    final currentUsages = stateOrNull ?? {};
    final newUsages = ref.watch(vmInfosProvider.select((infos) {
      return Map.fromIterables(
        infos.map((i) => i.name),
        infos.map((i) => i.instanceInfo.cpuUsage),
      );
    }));

    final updatedUsages = newUsages.entries.map((e) {
      final name = e.key;
      final usage = e.value;
      final usages = currentUsages
          .lookup(name)
          .getOrElse(() => Queue.of(Iterable.generate(100, (_) => 0.0)))
        ..removeFirst()
        ..addLast(usage);

      return MapEntry(name, usages);
    });

    return Map.fromEntries(updatedUsages);
  }
}

final cpuUsagesProvider =
    NotifierProvider.autoDispose<CpuUsagesNotifier, Map<String, Queue<double>>>(
        CpuUsagesNotifier.new);
