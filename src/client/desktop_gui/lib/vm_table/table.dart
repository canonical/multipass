import 'dart:math';

import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:two_dimensional_scrollables/two_dimensional_scrollables.dart';

class TableHeader<T> {
  final String name;
  final Widget Function(String) childBuilder;
  final String Function(T)? sortKey;
  double width;
  final double minWidth;
  double _oldWidth;
  final Widget Function(T) cellBuilder;

  TableHeader({
    this.name = '',
    required this.childBuilder,
    this.sortKey,
    required this.width,
    required this.minWidth,
    required this.cellBuilder,
  }) : _oldWidth = width;
}

class Table<T> extends ConsumerStatefulWidget {
  final List<TableHeader<T>> headers;
  final List<T> data;
  final List<Widget> finalRow;

  const Table({
    super.key,
    required this.headers,
    required this.data,
    required this.finalRow,
  });

  @override
  ConsumerState<Table<T>> createState() => _TableState<T>();
}

class _TableState<T> extends ConsumerState<Table<T>> {
  final horizontal = ScrollController();
  final vertical = ScrollController();
  var isResizingColumn = 0;
  bool sortAscending = false;
  int? sortIndex;

  static const borderSide = BorderSide(color: Colors.grey, width: 0.5);

  Widget addScrollbars(TableView table) {
    return Scrollbar(
      controller: vertical,
      child: Scrollbar(
        controller: horizontal,
        child: table,
      ),
    );
  }

  Widget buildHeader(int index, TableHeader<T> header) {
    final resizeHandle = MouseRegion(
      onEnter: (_) => setState(() => isResizingColumn++),
      onExit: (_) => setState(() => isResizingColumn--),
      child: GestureDetector(
        child: Container(
          width: 10,
          margin: const EdgeInsets.symmetric(vertical: 10),
          decoration: const BoxDecoration(
            color: Colors.transparent,
            border: Border(right: borderSide),
          ),
        ),
        onHorizontalDragStart: (_) => setState(() => isResizingColumn++),
        onHorizontalDragUpdate: (d) => setState(() {
          header.width = max(
            header.minWidth,
            header._oldWidth + d.localPosition.dx,
          );
        }),
        onHorizontalDragEnd: (_) => setState(() {
          header._oldWidth = header.width;
          isResizingColumn--;
        }),
      ),
    );

    final title = Stack(children: [
      Positioned.fill(child: header.childBuilder(header.name)),
      if (index == sortIndex)
        Align(
          alignment: Alignment.centerRight,
          child: sortAscending
              ? const Icon(Icons.arrow_drop_up_rounded)
              : const Icon(Icons.arrow_drop_down_rounded),
        ),
    ]);

    setSorting() {
      setState(() {
        if (sortIndex == index && !sortAscending) {
          sortAscending = false;
          sortIndex = null;
        } else {
          sortAscending = sortIndex == index ? !sortAscending : true;
          sortIndex = index;
        }
      });
    }

    return Stack(children: [
      header.sortKey != null ? InkWell(onTap: setSorting, child: title) : title,
      Align(alignment: Alignment.centerRight, child: resizeHandle),
    ]);
  }

  List<Widget> buildRow(T entry) {
    return [
      for (final h in widget.headers)
        Container(
          alignment: Alignment.centerLeft,
          margin: const EdgeInsets.all(10),
          child: h.cellBuilder(entry),
        ),
    ];
  }

  @override
  Widget build(BuildContext context) {
    Iterable<T> data = widget.data;
    final sortKey = widget.headers
        .elementAtOrNull(sortIndex ?? widget.headers.length)
        ?.sortKey;
    if (sortKey != null) {
      final sortedData = data.sortedBy(sortKey);
      data = sortAscending ? sortedData : sortedData.reversed;
    }

    final headerCells = [
      for (final (i, header) in widget.headers.indexed) buildHeader(i, header),
    ];
    final cells = [
      headerCells,
      ...data.map(buildRow),
      widget.finalRow,
    ];

    final table = TableView.builder(
      horizontalDetails: ScrollableDetails.horizontal(controller: horizontal),
      verticalDetails: ScrollableDetails.vertical(controller: vertical),
      pinnedRowCount: 1,
      rowCount: cells.length,
      columnCount: widget.headers.length + 1,
      rowBuilder: (_) => const TableSpan(extent: FixedTableSpanExtent(50)),
      columnBuilder: (i) => TableSpan(
        extent: i == widget.headers.length
            ? const RemainingTableSpanExtent()
            : FixedTableSpanExtent(widget.headers[i].width),
      ),
      cellBuilder: (_, v) => Container(
        decoration: BoxDecoration(
          border: Border(
            top: v.row == 0 ? borderSide : BorderSide.none,
            right: v.column == widget.headers.length
                ? borderSide
                : BorderSide.none,
            bottom: borderSide,
            left: v.column == 0 ? borderSide : BorderSide.none,
          ),
        ),
        child: cells.elementAtOrNull(v.row)?.elementAtOrNull(v.column),
      ),
    );

    return MouseRegion(
      cursor: isResizingColumn == 0
          ? MouseCursor.defer
          : SystemMouseCursors.resizeColumn,
      child: addScrollbars(table),
    );
  }
}
