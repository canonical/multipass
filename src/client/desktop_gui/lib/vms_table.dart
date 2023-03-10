import 'package:collection/collection.dart';
import 'package:data_table_2/data_table_2.dart';
import 'package:filesize/filesize.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'generated/multipass.pbgrpc.dart';
import 'globals.dart';
import 'instance_actions_button.dart';
import 'vms_screen.dart';

class VMsTable extends ConsumerStatefulWidget {
  const VMsTable({super.key});

  @override
  ConsumerState<VMsTable> createState() => _VMsTableState();
}

class _VMsTableState extends ConsumerState<VMsTable> {
  int? sortIndex;
  bool sortAscending = true;
  String Function(InfoReply_Info)? sortExtractor;

  DataColumnSortCallback _extractorToSortCallback(
    String Function(InfoReply_Info) extractor,
  ) =>
      (index, ascending) => setState(() {
            sortIndex = (sortIndex == index && ascending) ? null : index;
            sortAscending = ascending;
            sortExtractor = extractor;
          });

  late final _headers = {
    const DataColumn2(label: Text('Name')): (InfoReply_Info info) => info.name,
    const DataColumn2(label: Text('State'), fixedWidth: 130):
        (InfoReply_Info info) => info.instanceStatus.status.name,
    const DataColumn2(label: Text('Memory usage'), numeric: true): null,
    const DataColumn2(label: Text('Disk usage'), numeric: true): null,
    const DataColumn2(label: Text('CPU(s)'), size: ColumnSize.S, numeric: true):
        (InfoReply_Info info) => info.cpuCount,
    const DataColumn2(label: Text('Load'), numeric: true): null,
    const DataColumn2(label: Text('Image'), size: ColumnSize.L):
        (InfoReply_Info info) => info.currentRelease,
    const DataColumn2(label: Text('Ipv4')): (InfoReply_Info info) =>
        info.ipv4.firstOrNull ?? '',
    const DataColumn2(label: Text('Actions'), fixedWidth: 75, numeric: true):
        null,
  }.entries.map((e) => DataColumn2(
        label: e.key.label,
        size: e.key.size,
        numeric: e.key.numeric,
        fixedWidth: e.key.fixedWidth,
        onSort: e.value == null ? null : _extractorToSortCallback(e.value!),
      ));

  DataRow2 _buildRow(InfoReply_Info info, bool selected) {
    return DataRow2(
      selected: selected,
      onSelectChanged: (select) => ref
          .read(selectedVMsProvider.notifier)
          .update((selected) => select!
              ? ({...selected}..[info.name] = info)
              : ({...selected}..remove(info.name))),
      cells: [
        DataCell(Text(info.name)),
        DataCell(Text(info.instanceStatus.status.name.toLowerCase())),
        DataCell(Text(info.memoryUsage.isEmpty
            ? 'N/A'
            : '${filesize(info.memoryUsage)}/${filesize(info.memoryTotal)}')),
        DataCell(Text(info.diskUsage.isEmpty
            ? 'N/A'
            : '${filesize(info.diskUsage)}/${filesize(info.diskTotal)}')),
        DataCell(Text(info.cpuCount.isEmpty ? 'N/A' : info.cpuCount)),
        DataCell(Text(info.load.isEmpty ? 'N/A' : info.load)),
        DataCell(Text(info.currentRelease)),
        DataCell(Text(info.ipv4.firstOrNull ?? 'N/A')),
        DataCell(InstanceActionsButton(info.instanceStatus.status, info.name)),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    dataTableShowLogs = false;

    final runningOnly = ref.watch(runningOnlyProvider);
    final searchName = ref.watch(searchNameProvider);
    final selectedVMs = ref.watch(selectedVMsProvider);
    final infos = ref.watch(infoStreamProvider).when(
        data: (reply) => reply.info,
        loading: () => null,
        error: (error, stackTrace) {
          print(error);
          print(stackTrace);
          return ref.read(infoStreamProvider).valueOrNull?.info ?? [];
        });

    final emptyWidget = infos == null
        ? const Align(
            alignment: Alignment.topCenter,
            child: LinearProgressIndicator(),
          )
        : const Center(child: Text("No instances"));

    final filteredInfos = (infos ?? [])
        .where((info) => info.name.contains(searchName))
        .where((info) =>
            !runningOnly || info.instanceStatus.status == Status.RUNNING);

    final sortedInfos = sortIndex == null
        ? filteredInfos
        : filteredInfos.sorted((a, b) =>
            (sortAscending ? 1 : -1) *
            sortExtractor!(a).compareTo(sortExtractor!(b)));

    return DataTable2(
      empty: emptyWidget,
      sortColumnIndex: sortIndex,
      sortAscending: sortAscending,
      onSelectAll: (select) => ref.read(selectedVMsProvider.notifier).state =
          select! ? {for (final info in sortedInfos) info.name: info} : {},
      columns: _headers.toList(),
      rows: sortedInfos
          .map((info) => _buildRow(info, selectedVMs.containsKey(info.name)))
          .toList(),
    );
  }
}
