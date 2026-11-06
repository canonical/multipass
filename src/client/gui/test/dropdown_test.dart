import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/dropdown.dart';

void main() {
  const defaultItems = {'a': 'Item A', 'b': 'Item B', 'c': 'Item C'};

  Widget buildDropdown({
    String? label,
    String? value = 'a',
    ValueChanged<String?>? onChanged,
    Map<String, String> items = defaultItems,
    double width = 360,
    String? errorText,
    bool enabled = true,
  }) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Dropdown<String>(
            label: label,
            value: value,
            onChanged: onChanged ?? (_) {},
            items: items,
            width: width,
            errorText: errorText,
            enabled: enabled,
          ),
        ),
      ),
    );
  }

  testWidgets('shows the text of the selected item', (tester) async {
    await tester.pumpWidget(buildDropdown(value: 'b'));

    expect(find.text('Item B'), findsOneWidget);
    expect(find.text('Item A'), findsNothing);
  });

  testWidgets('shows the label when provided', (tester) async {
    await tester.pumpWidget(buildDropdown(label: 'My Label'));

    expect(find.text('My Label'), findsOneWidget);
  });

  testWidgets('shows only the selected item text when label is null',
      (tester) async {
    await tester.pumpWidget(buildDropdown());

    expect(find.byType(Text), findsOneWidget);
  });

  testWidgets('sizes the field to the given width', (tester) async {
    await tester.pumpWidget(buildDropdown(width: 200));

    expect(tester.getSize(find.byType(InputDecorator)).width, 200);
  });

  testWidgets('shows errorText when provided', (tester) async {
    await tester.pumpWidget(buildDropdown(errorText: 'Something is wrong'));

    expect(find.text('Something is wrong'), findsOneWidget);
  });

  testWidgets('invokes onChanged with the key of the tapped item',
      (tester) async {
    String? changedValue;
    await tester.pumpWidget(buildDropdown(onChanged: (v) => changedValue = v));

    await tester.tap(find.byType(DropdownButton<String>));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Item B').last);
    await tester.pumpAndSettle();

    expect(changedValue, 'b');
  });

  testWidgets('does not open the menu or invoke onChanged when disabled',
      (tester) async {
    var changeCount = 0;
    await tester.pumpWidget(buildDropdown(
      enabled: false,
      onChanged: (_) => changeCount++,
    ));

    await tester.tap(find.byType(DropdownButton<String>));
    await tester.pumpAndSettle();

    expect(find.text('Item B'), findsNothing);
    expect(changeCount, 0);
  });

  testWidgets('shows no selection when value is not one of the items',
      (tester) async {
    await tester.pumpWidget(buildDropdown(value: 'missing'));

    expect(tester.takeException(), isNull);
    expect(find.byType(Text), findsNothing);
  });
}
