import 'package:collection/collection.dart';
import 'package:data_table_2/data_table_2.dart';
import 'package:filesize/filesize.dart';
import 'package:flutter/material.dart';

import 'generated/multipass.pbgrpc.dart';
import 'instance_actions_button.dart';

class VMsTable extends StatefulWidget {
  final Iterable<InfoReply_Info>? infos;
  final ValueSetter<Map<String, InfoReply_Info>>? onSelect;
  final Map<String, InfoReply_Info> selected;

  const VMsTable({
    super.key,
    this.infos,
    this.selected = const {},
    this.onSelect,
  });

  @override
  State<VMsTable> createState() => _VMsTableState();
}

class _VMsTableState extends State<VMsTable> {
  int? sortIndex;
  bool sortAscending = true;
  String Function(InfoReply_Info)? sortExtractor;

  DataColumnSortCallback _extractorToSortCallback(
    String Function(InfoReply_Info)? extractor,
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
        onSort: e.value == null ? null : _extractorToSortCallback(e.value),
      ));

  Iterable<InfoReply_Info> get sortedInfos => sortExtractor == null
      ? widget.infos ?? []
      : (widget.infos ?? []).sorted((a, b) =>
          (sortIndex != null ? 1 : 0) *
          (sortAscending ? 1 : -1) *
          sortExtractor!(a).compareTo(sortExtractor!(b)));

  DataRow2 _buildRow(InfoReply_Info info) {
    return DataRow2(
      selected: widget.selected.containsKey(info.name),
      onSelectChanged: (select) => widget.onSelect?.call(
        select!
            ? ({...widget.selected}..[info.name] = info)
            : ({...widget.selected}..remove(info.name)),
      ),
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
    final infos = sortedInfos;

    return DataTable2(
      empty: widget.infos == null
          ? const Align(
              alignment: Alignment.topCenter,
              child: LinearProgressIndicator(),
            )
          : const Center(child: Text("No instances")),
      sortColumnIndex: sortIndex,
      sortAscending: sortAscending,
      onSelectAll: (select) => widget.onSelect?.call(
        select! ? {for (final info in infos) info.name: info} : {},
      ),
      columns: _headers.toList(),
      rows: infos.map(_buildRow).toList(),
    );
  }
}
