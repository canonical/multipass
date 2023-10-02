import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import 'cell_builders.dart';
import 'cpu_sparkline.dart';
import 'memory_usage.dart';
import 'table.dart';
import 'vms.dart';

final headers = <TableHeader<VmInfo>>[
  TableHeader(
    name: 'checkbox',
    childBuilder: (_) {
      return Consumer(builder: (_, ref, __) {
        final selected = ref.watch(selectedVmsProvider);
        final available = ref.watch(vmNamesProvider);
        final allSelected = selected.containsAll(available);

        return Center(
          child: Checkbox(
            tristate: true,
            value: selected.isEmpty ? false : (allSelected ? true : null),
            onChanged: (isSelected) {
              ref.read(selectedVmsProvider.notifier).state =
                  (isSelected ?? false) ? available.toBuiltSet() : BuiltSet();
            },
          ),
        );
      });
    },
    width: 50,
    minWidth: 50,
    cellBuilder: (info) {
      return Consumer(builder: (_, ref, __) {
        final selected = ref.watch(selectedVmsProvider);

        return Center(
          child: Checkbox(
            value: selected.contains(info.name),
            onChanged: (isSelected) {
              ref.read(selectedVmsProvider.notifier).update((state) {
                final builder = state.toBuilder();
                isSelected!
                    ? builder.add(info.name)
                    : builder.remove(info.name);
                return builder.build();
              });
            },
          ),
        );
      });
    },
  ),
  TableHeader(
    name: 'NAME',
    childBuilder: headerBuilder,
    width: 115,
    minWidth: 70,
    sortKey: (info) => info.name,
    cellBuilder: (info) => ellipsizedText(info.name),
  ),
  TableHeader(
    name: 'STATE',
    childBuilder: headerBuilder,
    width: 110,
    minWidth: 70,
    sortKey: (info) => info.instanceStatus.status.name,
    cellBuilder: (info) => vmStatus(info.instanceStatus.status),
  ),
  TableHeader(
    name: 'CPU USAGE',
    childBuilder: headerBuilder,
    width: 130,
    minWidth: 100,
    cellBuilder: (info) => CpuSparkline(cpuUsages[info.name]!),
  ),
  TableHeader(
    name: 'MEMORY USAGE',
    childBuilder: headerBuilder,
    width: 140,
    minWidth: 130,
    cellBuilder: (info) => MemoryUsage(
      used: info.instanceInfo.memoryUsage,
      total: info.memoryTotal,
    ),
  ),
  TableHeader(
    name: 'DISK USAGE',
    childBuilder: headerBuilder,
    width: 130,
    minWidth: 100,
    cellBuilder: (info) => MemoryUsage(
      used: info.instanceInfo.diskUsage,
      total: info.diskTotal,
    ),
  ),
  TableHeader(
    name: 'PRIVATE IP',
    childBuilder: headerBuilder,
    width: 140,
    minWidth: 100,
    cellBuilder: (info) => ipAddresses(info.instanceInfo.ipv4.take(1)),
  ),
  TableHeader(
    name: 'PUBLIC IP',
    childBuilder: headerBuilder,
    width: 140,
    minWidth: 100,
    cellBuilder: (info) => ipAddresses(info.instanceInfo.ipv4.skip(1)),
  ),
];
