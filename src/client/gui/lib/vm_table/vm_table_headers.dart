import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../extensions.dart';
import '../providers.dart';
import '../sidebar.dart';
import '../vm_details/cpu_sparkline.dart';
import '../vm_details/ip_addresses.dart';
import '../vm_details/memory_usage.dart';
import '../vm_details/vm_status_icon.dart';
import 'table.dart';
import 'vms.dart';

final headers = <TableHeader<VmInfo>>[
  TableHeader(
    name: 'checkbox',
    childBuilder: (_) => const SelectAllCheckbox(),
    width: 50,
    minWidth: 50,
    cellBuilder: (info) => SelectVmCheckbox(info.name),
  ),
  TableHeader(
    name: 'NAME',
    width: 115,
    minWidth: 70,
    sortKey: (info) => info.name,
    cellBuilder: (info) => VmNameLink(info.name),
  ),
  TableHeader(
    name: 'STATE',
    width: 110,
    minWidth: 70,
    sortKey: (info) => info.instanceStatus.status.name,
    cellBuilder: (info) => VmStatusIcon(info.instanceStatus.status),
  ),
  TableHeader(
    name: 'CPU USAGE',
    width: 130,
    minWidth: 100,
    cellBuilder: (info) => CpuSparkline(info.name),
  ),
  TableHeader(
    name: 'MEMORY USAGE',
    width: 140,
    minWidth: 130,
    cellBuilder: (info) => MemoryUsage(
      used: info.instanceInfo.memoryUsage,
      total: info.memoryTotal,
    ),
  ),
  TableHeader(
    name: 'DISK USAGE',
    width: 130,
    minWidth: 100,
    cellBuilder: (info) => MemoryUsage(
      used: info.instanceInfo.diskUsage,
      total: info.diskTotal,
    ),
  ),
  TableHeader(
    name: 'PRIVATE IP',
    width: 140,
    minWidth: 100,
    cellBuilder: (info) => IpAddresses(info.instanceInfo.ipv4.take(1)),
  ),
  TableHeader(
    name: 'PUBLIC IP',
    width: 140,
    minWidth: 100,
    cellBuilder: (info) => IpAddresses(info.instanceInfo.ipv4.skip(1)),
  ),
];

class SelectAllCheckbox extends ConsumerWidget {
  const SelectAllCheckbox({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedVms = ref.watch(selectedVmsProvider);
    final vmNames = ref.watch(vmNamesProvider);
    final allSelected = selectedVms.containsAll(vmNames);

    void toggleSelectedAll(bool isSelected) {
      final newState = isSelected ? vmNames.toBuiltSet() : BuiltSet<String>();
      ref.read(selectedVmsProvider.notifier).state = newState;
    }

    return Center(
      child: Checkbox(
        tristate: true,
        value: selectedVms.isEmpty ? false : (allSelected ? true : null),
        onChanged: (checked) => toggleSelectedAll(checked ?? false),
      ),
    );
  }
}

class SelectVmCheckbox extends ConsumerWidget {
  final String name;

  const SelectVmCheckbox(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selected = ref.watch(selectedVmsProvider.select((selectedVms) {
      return selectedVms.contains(name);
    }));

    void toggleSelected(bool isSelected) {
      ref.read(selectedVmsProvider.notifier).update((state) {
        return state.rebuild((set) {
          isSelected ? set.add(name) : set.remove(name);
        });
      });
    }

    return Center(
      child: Checkbox(
        value: selected,
        onChanged: (checked) => toggleSelected(checked!),
      ),
    );
  }
}

class VmNameLink extends ConsumerWidget {
  final String name;

  const VmNameLink(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    goToVm() => ref.read(sidebarKeyProvider.notifier).state = 'vm-$name';

    return Tooltip(
      message: name,
      child: Text.rich(
        name.nonBreaking.span.link(ref, goToVm),
        overflow: TextOverflow.ellipsis,
      ),
    );
  }
}
