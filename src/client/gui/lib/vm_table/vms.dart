import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart' hide Table, Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../catalogue/catalogue.dart';
import '../providers.dart';
import '../sidebar.dart';
import '../switch.dart';
import '../vm_details/memory_usage.dart';
import 'bulk_actions.dart';
import 'header_selection.dart';
import 'search_box.dart';
import 'table.dart';
import 'vm_table_headers.dart';

final runningOnlyProvider = StateProvider((_) => false);
final selectedVmsProvider = StateProvider<BuiltSet<String>>((ref) {
  // if any filter is applied (either name or show running only), the provider
  // will be invalidated and return the empty set again
  ref.watch(runningOnlyProvider);
  ref.watch(searchNameProvider);
  // if navigating to another page, deselect all
  ref.watch(sidebarKeyProvider);
  // look for changes in available vms and make sure this set does not contain
  // vm names that are no longer present
  ref.listen(vmNamesProvider, (_, availableNames) {
    ref.controller.update(availableNames.intersection);
  });

  return BuiltSet();
});

class Vms extends ConsumerWidget {
  const Vms({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    goToCatalogue() {
      ref.read(sidebarKeyProvider.notifier).set(CatalogueScreen.sidebarKey);
    }

    final heading = Row(children: [
      const Expanded(
        child: Text(
          'All Instances',
          style: TextStyle(fontSize: 37, fontWeight: FontWeight.w300),
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
        ),
      ),
      TextButton(
        onPressed: goToCatalogue,
        child: const Text('Launch'),
      )
    ]);

    final searchName = ref.watch(searchNameProvider);
    final runningOnly = ref.watch(runningOnlyProvider);
    final vmFilters = Row(children: [
      Switch(
        label: 'Show running instances only',
        value: runningOnly,
        onChanged: (v) => ref.read(runningOnlyProvider.notifier).state = v,
      ),
      const Spacer(),
      const SearchBox(),
      const SizedBox(width: 8),
      const HeaderSelection(),
    ]);

    final enabledHeaderNames = ref.watch(enabledHeadersProvider).asMap();
    final enabledHeaders =
        headers.where((h) => enabledHeaderNames[h.name]!).toList();

    final infos = ref
        .watch(vmInfosProvider)
        .where((i) => !runningOnly || i.instanceStatus.status == Status.RUNNING)
        .where((i) => i.name.contains(searchName))
        .toList();

    int total(Iterable<String> it) => it.map((e) => int.tryParse(e) ?? 0).sum;
    final totalUsedMemory = total(infos.map((i) => i.instanceInfo.memoryUsage));
    final totalTotalMemory = total(infos.map((i) => i.memoryTotal));
    final totalUsedDisk = total(infos.map((i) => i.instanceInfo.diskUsage));
    final totalTotalDisk = total(infos.map((i) => i.diskTotal));
    final totalUsageRow = [
      const SizedBox.shrink(),
      Container(
        margin: const EdgeInsets.all(10),
        alignment: Alignment.centerLeft,
        child: const Text(
          "Total",
          style: TextStyle(fontWeight: FontWeight.bold),
        ),
      ),
      for (final name in enabledHeaderNames.whereValue((e) => e).keys.skip(2))
        if (name == 'MEMORY USAGE')
          Container(
            margin: const EdgeInsets.all(10),
            child: MemoryUsage(
              used: '$totalUsedMemory',
              total: '$totalTotalMemory',
            ),
          )
        else if (name == 'DISK USAGE')
          Container(
            margin: const EdgeInsets.all(10),
            child: MemoryUsage(
              used: '$totalUsedDisk',
              total: '$totalTotalDisk',
            ),
          )
        else
          const SizedBox.shrink(),
    ];

    return Padding(
      padding: const EdgeInsets.all(20).copyWith(top: 52),
      child: Column(children: [
        heading,
        const SizedBox(height: 35),
        vmFilters,
        const BulkActionsBar(),
        const SizedBox(height: 10),
        Flexible(
          child: SizedBox(
            height: (infos.length + 2) * 50,
            child: Table<VmInfo>(
              headers: enabledHeaders,
              data: infos.toList(),
              finalRow: totalUsageRow,
            ),
          ),
        ),
      ]),
    );
  }
}
