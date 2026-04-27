import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/dropdown.dart';

void main() {
  final stringItems = {'a': 'Item A', 'b': 'Item B', 'c': 'Item C'};
  final intItems = {1: 'One', 2: 'Two', 3: 'Three'};

  Widget buildDropdown<T>({
    String? label,
    T? value,
    required ValueChanged<T?> onChanged,
    required Map<T, String> items,
    double width = 360,
  }) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Dropdown<T>(
            label: label,
            value: value,
            onChanged: onChanged,
            items: items,
            width: width,
          ),
        ),
      ),
    );
  }

  group('Dropdown', () {
    group('rendering', () {
      testWidgets('renders the selected item label', (tester) async {
        await tester.pumpWidget(buildDropdown<String>(
          value: 'b',
          onChanged: (_) {},
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        expect(find.text('Item B'), findsOneWidget);
      });

      testWidgets('renders label text when label is provided', (tester) async {
        await tester.pumpWidget(buildDropdown<String>(
          label: 'My Label',
          value: 'a',
          onChanged: (_) {},
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        expect(find.text('My Label'), findsOneWidget);
      });

      testWidgets('does not render a label Text when label is null',
          (tester) async {
        await tester.pumpWidget(buildDropdown<String>(
          value: 'a',
          onChanged: (_) {},
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        // Only the selected item text should be present — no extra Text for a label
        expect(find.text('Item A'), findsOneWidget);
        expect(find.text('Item B'), findsNothing);
        expect(find.text('Item C'), findsNothing);
      });

      testWidgets('renders DropdownButton in the widget tree', (tester) async {
        await tester.pumpWidget(buildDropdown<String>(
          value: 'a',
          onChanged: (_) {},
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        expect(find.byType(DropdownButton<String>), findsOneWidget);
      });
    });

    group('initial value', () {
      testWidgets('DropdownButton has the correct initial value',
          (tester) async {
        await tester.pumpWidget(buildDropdown<String>(
          value: 'c',
          onChanged: (_) {},
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        final button = tester.widget<DropdownButton<String>>(
            find.byType(DropdownButton<String>));
        expect(button.value, equals('c'));
      });
    });

    group('interaction', () {
      testWidgets(
          'onChanged is called with the selected value when a new item is tapped',
          (tester) async {
        String? changedValue;
        await tester.pumpWidget(buildDropdown<String>(
          value: 'a',
          onChanged: (v) => changedValue = v,
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        await tester.tap(find.byType(DropdownButton<String>));
        await tester.pumpAndSettle();

        await tester.tap(find.text('Item B').last);
        await tester.pumpAndSettle();

        expect(changedValue, equals('b'));
      });
    });

    group('generic type support', () {
      testWidgets('works with String type items', (tester) async {
        await tester.pumpWidget(buildDropdown<String>(
          value: 'a',
          onChanged: (_) {},
          items: stringItems,
        ));
        await tester.pumpAndSettle();

        expect(find.byType(DropdownButton<String>), findsOneWidget);
        expect(find.text('Item A'), findsOneWidget);
      });

      testWidgets('works with int type items', (tester) async {
        await tester.pumpWidget(buildDropdown<int>(
          value: 2,
          onChanged: (_) {},
          items: intItems,
        ));
        await tester.pumpAndSettle();

        expect(find.byType(DropdownButton<int>), findsOneWidget);
        expect(find.text('Two'), findsOneWidget);
      });

      testWidgets('onChanged receives int value when int item is selected',
          (tester) async {
        int? changedValue;
        await tester.pumpWidget(buildDropdown<int>(
          value: 1,
          onChanged: (v) => changedValue = v,
          items: intItems,
        ));
        await tester.pumpAndSettle();

        await tester.tap(find.byType(DropdownButton<int>));
        await tester.pumpAndSettle();

        await tester.tap(find.text('Three').last);
        await tester.pumpAndSettle();

        expect(changedValue, equals(3));
      });
    });
  });
}
