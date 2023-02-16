import 'package:data_table_2/data_table_2.dart';
import 'package:filesize/filesize.dart';
import 'package:flutter/material.dart';

import 'generated/multipass.pb.dart';

class VMsTable extends StatefulWidget {
  final Iterable<InfoReply_Info> infos;

  const VMsTable({super.key, required this.infos});

  @override
  State<VMsTable> createState() => _VMsTableState();
}

class _VMsTableState extends State<VMsTable> {
  final selected = <String>{};
  int? sortIndex;
  bool sortAscending = true;
  String Function(InfoReply_Info)? sortByExtractor;

  final tableHeaders = {
    'Name': (InfoReply_Info info) => info.name,
    'Status': (InfoReply_Info info) => info.instanceStatus.status.name,
    'Usage': (InfoReply_Info info) => info.diskUsage,
    'Total': (InfoReply_Info info) => info.diskTotal,
    'Image': (InfoReply_Info info) => info.currentRelease,
    'Ipv4': (InfoReply_Info info) => info.ipv4.isEmpty ? '' : info.ipv4.first,
    'Mounts': null,
    'Actions': null,
  };

  Iterable<InfoReply_Info> get sortedInfos => sortByExtractor == null
      ? widget.infos
      : (widget.infos.toList()
        ..sort((a, b) =>
            (sortIndex != null ? 1 : 0) *
            (sortAscending ? 1 : -1) *
            sortByExtractor!(a).compareTo(sortByExtractor!(b))));

  @override
  Widget build(BuildContext context) {
    return DataTable2(
      sortColumnIndex: sortIndex,
      sortAscending: sortAscending,
      columns: [
        for (final e in tableHeaders.entries)
          DataColumn2(
            label: Text(e.key),
            onSort: (index, ascending) => setState(() {
              sortIndex = (sortIndex == index && ascending) ? null : index;
              sortAscending = ascending;
              sortByExtractor = e.value;
            }),
          )
      ],
      rows: [
        for (final info in sortedInfos)
          DataRow2(
            selected: selected.contains(info.name),
            onSelectChanged: (select) => setState(() {
              select! ? selected.add(info.name) : selected.remove(info.name);
            }),
            cells: [
              DataCell(Text(info.name)),
              DataCell(Text(info.instanceStatus.status.name.toLowerCase())),
              DataCell(Text(
                  info.diskUsage.isEmpty ? 'N/A' : filesize(info.diskUsage))),
              DataCell(Text(
                  info.diskTotal.isEmpty ? 'N/A' : filesize(info.diskTotal))),
              DataCell(Text(info.currentRelease)),
              DataCell(Text(info.ipv4.isEmpty ? 'N/A' : info.ipv4.first)),
              DataCell(Text(info.mountInfo.mountPaths.isNotEmpty
                  ? info.mountInfo.mountPaths.first.sourcePath
                  : '')),
              DataCell(IconButton(
                splashRadius: 20,
                icon: const Icon(Icons.more_vert),
                onPressed: () => print(info),
              )),
            ],
          )
      ],
    );
  }
}
