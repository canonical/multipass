import 'package:flutter/material.dart' hide Table;
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_table/table.dart';

/// Builds a [Table<String>] with a single sortable "Name" column and an
/// optional non-sortable "Static" column.
Widget buildTable({
  List<String> data = const ['banana', 'apple', 'cherry'],
  bool includeNonSortable = false,
}) {
  final headers = <TableHeader<String>>[
    TableHeader<String>(
      name: 'Name',
      sortKey: (s) => s,
      width: 120,
      minWidth: 60,
      cellBuilder: (s) => Text(s),
    ),
    if (includeNonSortable)
      TableHeader<String>(
        name: 'Static',
        width: 100,
        minWidth: 60,
        cellBuilder: (s) => Text(s.toUpperCase()),
      ),
  ];

  return MaterialApp(
    home: Scaffold(
      body: SizedBox(
        width: 500,
        height: 300,
        child: Table<String>(
          headers: headers,
          data: data,
          finalRow: const [],
        ),
      ),
    ),
  );
}

void main() {
  group('Table sort cycling', () {
    testWidgets('initially no sort arrow is shown', (tester) async {
      await tester.pumpWidget(buildTable());
      await tester.pump();

      expect(find.byIcon(Icons.arrow_drop_up_rounded), findsNothing);
      expect(find.byIcon(Icons.arrow_drop_down_rounded), findsNothing);
    });

    testWidgets('first tap on sortable header shows ascending arrow',
        (tester) async {
      await tester.pumpWidget(buildTable());
      await tester.pump();

      await tester.tap(find.text('Name'));
      await tester.pump();

      expect(find.byIcon(Icons.arrow_drop_up_rounded), findsOneWidget);
      expect(find.byIcon(Icons.arrow_drop_down_rounded), findsNothing);
    });

    testWidgets('second tap shows descending arrow', (tester) async {
      await tester.pumpWidget(buildTable());
      await tester.pump();

      await tester.tap(find.text('Name'));
      await tester.pump();
      await tester.tap(find.text('Name'));
      await tester.pump();

      expect(find.byIcon(Icons.arrow_drop_down_rounded), findsOneWidget);
      expect(find.byIcon(Icons.arrow_drop_up_rounded), findsNothing);
    });

    testWidgets('third tap removes the sort arrow', (tester) async {
      await tester.pumpWidget(buildTable());
      await tester.pump();

      await tester.tap(find.text('Name'));
      await tester.pump();
      await tester.tap(find.text('Name'));
      await tester.pump();
      await tester.tap(find.text('Name'));
      await tester.pump();

      expect(find.byIcon(Icons.arrow_drop_up_rounded), findsNothing);
      expect(find.byIcon(Icons.arrow_drop_down_rounded), findsNothing);
    });

    testWidgets('tapping non-sortable header does not show a sort arrow',
        (tester) async {
      await tester.pumpWidget(buildTable(includeNonSortable: true));
      await tester.pump();

      await tester.tap(find.text('Static'));
      await tester.pump();

      expect(find.byIcon(Icons.arrow_drop_up_rounded), findsNothing);
      expect(find.byIcon(Icons.arrow_drop_down_rounded), findsNothing);
    });

    testWidgets('ascending sort renders data in alphabetical order',
        (tester) async {
      await tester.pumpWidget(buildTable());
      await tester.pump();

      await tester.tap(find.text('Name'));
      await tester.pump();

      final appleTop = tester.getRect(find.text('apple')).top;
      final bananaTop = tester.getRect(find.text('banana')).top;
      final cherryTop = tester.getRect(find.text('cherry')).top;
      expect(appleTop < bananaTop, isTrue);
      expect(bananaTop < cherryTop, isTrue);
    });

    testWidgets('descending sort renders data in reverse alphabetical order',
        (tester) async {
      await tester.pumpWidget(buildTable());
      await tester.pump();

      await tester.tap(find.text('Name'));
      await tester.pump();
      await tester.tap(find.text('Name'));
      await tester.pump();

      final appleTop = tester.getRect(find.text('apple')).top;
      final bananaTop = tester.getRect(find.text('banana')).top;
      final cherryTop = tester.getRect(find.text('cherry')).top;
      expect(cherryTop < bananaTop, isTrue);
      expect(bananaTop < appleTop, isTrue);
    });
  });
}
