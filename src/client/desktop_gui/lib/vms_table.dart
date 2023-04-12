import 'dart:convert';

import 'package:basics/iterable_basics.dart';
import 'package:collection/collection.dart';
import 'package:data_table_2/data_table_2.dart';
import 'package:filesize/filesize.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'grpc_client.dart';
import 'instance_actions_button.dart';
import 'settings.dart';
import 'vms_screen.dart';

const columnWidthsKey = 'columnWidths';

class TableHeader {
  final String title;
  double width;
  final double minWidth;
  final String Function(VmInfo)? sortExtractor;
  void Function(double)? resizeCallback;
  void Function()? resizeStartCallback;

  TableHeader({
    required this.title,
    required this.width,
    required this.minWidth,
    this.sortExtractor,
    this.resizeCallback,
    this.resizeStartCallback,
  });
}

class VMsTable extends ConsumerStatefulWidget {
  const VMsTable({super.key});

  @override
  ConsumerState<VMsTable> createState() => _VMsTableState();
}

class _VMsTableState extends ConsumerState<VMsTable> {
  int? sortIndex;
  bool sortAscending = true;
  String Function(VmInfo)? sortExtractor;

  double tableWidthFactor = 0;

  // the widths represent fractions out of the total sum
  // the minWidths are the minimum widths but in pixels!
  final headers = [
    TableHeader(
      title: 'Name',
      width: 115,
      minWidth: 60,
      sortExtractor: (VmInfo info) => info.name,
    ),
    TableHeader(
      title: 'State',
      width: 100,
      minWidth: 65,
      sortExtractor: (VmInfo info) => info.instanceStatus.status.name,
    ),
    TableHeader(title: 'Memory usage', width: 140, minWidth: 115),
    TableHeader(title: 'Disk usage', width: 130, minWidth: 90),
    TableHeader(
      title: 'CPU(s)',
      width: 80,
      minWidth: 75,
      sortExtractor: (VmInfo info) => info.cpuCount,
    ),
    TableHeader(title: 'Load', width: 100, minWidth: 50),
    TableHeader(
      title: 'Image',
      width: 125,
      minWidth: 70,
      sortExtractor: (VmInfo info) => info.currentRelease,
    ),
    TableHeader(
      title: 'Ipv4',
      width: 143,
      minWidth: 80,
      sortExtractor: (VmInfo info) => info.ipv4.firstOrNull ?? '',
    ),
    TableHeader(
      title: 'Actions',
      width: 67,
      minWidth: 60,
    ),
  ];

  double get headersWidth => headers.map((h) => h.width).sum;

  DataColumnSortCallback _extractorToSortCallback(
    String Function(VmInfo) extractor,
  ) =>
      (index, ascending) => setState(() {
            sortIndex = (sortIndex == index && ascending) ? null : index;
            sortAscending = ascending;
            sortExtractor = extractor;
          });

  Widget _textWithEllipsis(String text) {
    return Tooltip(
      message: text,
      child: Text(
        text.replaceAll('-', '\u2011').replaceAll(' ', '\u00A0'),
        overflow: TextOverflow.ellipsis,
      ),
    );
  }

  Widget _memoryUsage(String usage, String total) {
    final nUsage = double.tryParse(usage);
    final nTotal = double.tryParse(total);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.end,
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        LinearProgressIndicator(
          value: (nUsage ?? 0) / (nTotal ?? 1),
          backgroundColor: const Color(0xff006ada).withOpacity(0.24),
          color: const Color(0xff0066cc),
        ),
        Text(
          (nUsage != null && nTotal != null)
              ? '${filesize(usage)} / ${filesize(total)}'
              : '-',
          style: const TextStyle(fontSize: 11),
        ),
      ],
    );
  }

  Icon _statusIcon(Status status) {
    switch (status) {
      case Status.RUNNING:
        return const Icon(Icons.circle, color: Color(0xff0e8420), size: 10);
      case Status.STOPPED:
        return const Icon(Icons.circle, color: Color(0xff757575), size: 10);
      case Status.SUSPENDED:
        return const Icon(Icons.circle, color: Color(0xfff99b11), size: 10);
      case Status.RESTARTING:
        return const Icon(Icons.more_horiz, color: Color(0xff757575), size: 15);
      case Status.STARTING:
        return const Icon(Icons.more_horiz, color: Color(0xff757575), size: 15);
      case Status.SUSPENDING:
        return const Icon(Icons.more_horiz, color: Color(0xff757575), size: 15);
      case Status.DELETED:
        return const Icon(Icons.close, color: Colors.redAccent, size: 15);
      case Status.DELAYED_SHUTDOWN:
        return const Icon(Icons.circle, color: Color(0xff0e8420), size: 10);
      default:
        return const Icon(Icons.help, color: Color(0xff757575), size: 15);
    }
  }

  Widget _vmStatus(Status status) {
    final statusName = status.name.toLowerCase().replaceAll('_', ' ');
    return Row(children: [
      _statusIcon(status),
      const SizedBox(width: 8),
      Expanded(child: _textWithEllipsis(statusName)),
    ]);
  }

  Widget _vmIPs(List<String> ips) {
    return Row(children: [
      Expanded(child: _textWithEllipsis(ips.firstOrNull ?? '-')),
      if (ips.length > 1)
        Badge.count(
          count: ips.length - 1,
          alignment: const AlignmentDirectional(0, 10),
          textStyle: const TextStyle(fontSize: 8),
          largeSize: 14,
          child: PopupMenuButton<void>(
            icon: const Icon(Icons.keyboard_arrow_down),
            position: PopupMenuPosition.under,
            tooltip: 'Other IP addresses',
            itemBuilder: (_) => [
              const PopupMenuItem(
                enabled: false,
                labelTextStyle: MaterialStatePropertyAll(
                  TextStyle(color: Colors.black),
                ),
                child: Text('Other IP addresses'),
              ),
              for (final ip in ips.slice(1))
                PopupMenuItem(
                  enabled: false,
                  labelTextStyle: const MaterialStatePropertyAll(
                    TextStyle(color: Colors.black),
                  ),
                  child: Text(ip),
                ),
            ],
          ),
        ),
    ]);
  }

  DataRow2 _buildRow(VmInfo info, bool selected) {
    return DataRow2(
      selected: selected,
      onSelectChanged: (select) => ref
          .read(selectedVMsProvider.notifier)
          .update((selected) => select!
              ? ({...selected, info.name: info.instanceStatus.status})
              : ({...selected}..remove(info.name))),
      cells: [
        // first column header must not be padded, as it does not have a resize handle
        DataCell(_textWithEllipsis(info.name)),
        ...[
          _vmStatus(info.instanceStatus.status),
          _memoryUsage(info.memoryUsage, info.memoryTotal),
          _memoryUsage(info.diskUsage, info.diskTotal),
          Text(info.cpuCount.isEmpty ? '-' : info.cpuCount),
          _textWithEllipsis(info.load.isEmpty ? '-' : info.load),
          _textWithEllipsis(info.currentRelease),
          _vmIPs(info.ipv4),
          Align(
            alignment: Alignment.centerRight,
            child: InstanceActionsButton(info.instanceStatus.status, info.name),
          ),
        ].map((widget) => DataCell(Padding(
              padding: const EdgeInsets.symmetric(horizontal: 9),
              child: widget,
            )))
      ],
    );
  }

  List<DataColumn2> _buildColumns(Iterable<TableHeader> headers) {
    return headers.map((header) {
      return DataColumn2(
          fixedWidth: header.width * tableWidthFactor,
          onSort: header.sortExtractor != null
              ? _extractorToSortCallback(header.sortExtractor!)
              : null,
          label: Row(children: [
            if (header.resizeCallback != null)
              MouseRegion(
                cursor: SystemMouseCursors.resizeColumn,
                child: GestureDetector(
                  onPanStart: (_) => header.resizeStartCallback?.call(),
                  onPanEnd: (_) => saveColumnWidths(),
                  onPanUpdate: (details) => header.resizeCallback
                      ?.call(details.localPosition.dx / tableWidthFactor),
                  child: Container(
                    width: 10,
                    margin: const EdgeInsets.symmetric(vertical: 10),
                    decoration: const BoxDecoration(
                      color: Colors.transparent,
                      border: Border(
                        left: BorderSide(color: Colors.black26),
                      ),
                    ),
                  ),
                ),
              ),
            Expanded(child: Text(header.title)),
          ]));
    }).toList();
  }

  void saveColumnWidths() {
    final widths = Map.fromEntries(
      headers.map((h) => MapEntry(h.title, h.width)),
    );
    ref.read(settingsProvider).setString(columnWidthsKey, jsonEncode(widths));
  }

  void loadColumnWidths() {
    try {
      final widthsJson = ref.read(settingsProvider).getString(columnWidthsKey);
      // it's not an error if the setting is not present
      if (widthsJson == null) return;
      final decodedWidths = jsonDecode(widthsJson) as Map<String, dynamic>;
      final widths = decodedWidths.cast<String, double>();
      if (!headers.map((h) => h.title).all(widths.containsKey)) {
        throw "Couldn't load all column widths";
      }
      for (final header in headers) {
        header.width = widths[header.title]!;
      }
    } catch (e) {
      print('Failed to load column widths: $e');
    }
  }

  @override
  void initState() {
    super.initState();

    loadColumnWidths();
    for (final pair in IterableZip([headers, headers.skip(1)])) {
      final first = pair.first;
      final second = pair.last;
      double firstInitialWidth = 0;
      double secondInitialWidth = 0;
      second.resizeStartCallback = () {
        firstInitialWidth = first.width;
        secondInitialWidth = second.width;
      };
      second.resizeCallback = (dx) => setState(() {
            final leftWidth = firstInitialWidth + dx;
            final rightWidth = secondInitialWidth - dx;
            if (leftWidth >= first.minWidth / tableWidthFactor &&
                rightWidth >= second.minWidth / tableWidthFactor) {
              first.width = leftWidth;
              second.width = rightWidth;
            }
          });
    }
  }

  @override
  Widget build(BuildContext context) {
    dataTableShowLogs = false;

    final runningOnly = ref.watch(runningOnlyProvider);
    final searchName = ref.watch(searchNameProvider);
    final selectedVMs = ref.watch(selectedVMsProvider);
    final infos = ref.watch(vmInfosStreamProvider).when(
        data: (infos) => infos,
        loading: () => null,
        error: (error, stackTrace) {
          print(error);
          print(stackTrace);
          return ref.read(vmInfosStreamProvider).valueOrNull;
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

    return LayoutBuilder(builder: (context, BoxConstraints constraints) {
      // 91 is the amount of pixels from the total table width that come from
      // non-resizable elements e.g. checkboxes etc.
      tableWidthFactor = (constraints.maxWidth - 91) / headersWidth;
      return DataTable2(
        columnSpacing: 0,
        sortArrowIcon: Icons.arrow_drop_up,
        headingTextStyle: const TextStyle(
          fontWeight: FontWeight.bold,
          color: Colors.black,
        ),
        empty: emptyWidget,
        sortColumnIndex: sortIndex,
        sortAscending: sortAscending,
        onSelectAll: (select) => ref.read(selectedVMsProvider.notifier).state =
            select! ? infosToStatusMap(sortedInfos) : {},
        columns: _buildColumns(headers),
        rows: sortedInfos
            .map((info) => _buildRow(info, selectedVMs.containsKey(info.name)))
            .toList(),
      );
    });
  }
}
