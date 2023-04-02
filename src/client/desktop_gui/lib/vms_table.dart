import 'package:collection/collection.dart';
import 'package:data_table_2/data_table_2.dart';
import 'package:filesize/filesize.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'grpc_client.dart';
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
  String Function(VmInfo)? sortExtractor;

  DataColumnSortCallback _extractorToSortCallback(
    String Function(VmInfo) extractor,
  ) =>
      (index, ascending) => setState(() {
            sortIndex = (sortIndex == index && ascending) ? null : index;
            sortAscending = ascending;
            sortExtractor = extractor;
          });

  late final _headersWithSortExtractors = {
    const DataColumn2(label: Text('Name')): (VmInfo info) => info.name,
    const DataColumn2(label: Text('State'), fixedWidth: 200): (VmInfo info) =>
        info.instanceStatus.status.name,
    const DataColumn2(label: Text('Memory usage'), numeric: true): null,
    const DataColumn2(label: Text('Disk usage'), numeric: true): null,
    const DataColumn2(label: Text('CPU(s)'), fixedWidth: 120, numeric: true):
        (VmInfo info) => info.cpuCount,
    const DataColumn2(label: Text('Load'), numeric: true): null,
    const DataColumn2(label: Text('Image'), size: ColumnSize.L):
        (VmInfo info) => info.currentRelease,
    const DataColumn2(label: Text('Ipv4'), size: ColumnSize.L): (VmInfo info) =>
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
    return Row(
      children: [
        _statusIcon(status),
        const SizedBox(width: 8),
        Text(status.name.toLowerCase().replaceAll('_', ' ')),
      ],
    );
  }

  Widget _vmIPs(List<String> ips) {
    return Row(children: [
      Text(ips.firstOrNull ?? '-'),
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
        DataCell(Text(info.name)),
        DataCell(_vmStatus(info.instanceStatus.status)),
        DataCell(_memoryUsage(info.memoryUsage, info.memoryTotal)),
        DataCell(_memoryUsage(info.diskUsage, info.diskTotal)),
        DataCell(Text(info.cpuCount.isEmpty ? '-' : info.cpuCount)),
        DataCell(Text(info.load.isEmpty ? '-' : info.load)),
        DataCell(Text(info.currentRelease)),
        DataCell(_vmIPs(info.ipv4)),
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

    return DataTable2(
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
      columns: _headersWithSortExtractors.toList(),
      rows: sortedInfos
          .map((info) => _buildRow(info, selectedVMs.containsKey(info.name)))
          .toList(),
    );
  }
}
