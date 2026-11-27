import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/settings/setting_widgets.dart';

Widget _wrap(Widget child) => MaterialApp(
      home: Scaffold(body: SizedBox(width: 900, child: child)),
    );

void main() {
  testWidgets('SettingsSectionTitle renders its text in bold', (tester) async {
    await tester.pumpWidget(_wrap(const SettingsSectionTitle('Title')));

    final text = tester.widget<Text>(find.text('Title'));
    expect(text.style?.fontWeight, FontWeight.bold);
  });

  group('FixedOptionsSetting', () {
    Widget build({
      String? value,
      required List<String> changes,
      Widget? subtitle,
    }) {
      return _wrap(FixedOptionsSetting<String>(
        label: 'My label',
        options: const {'a': 'Alpha', 'b': 'Beta'},
        value: value,
        onChanged: changes.add,
        subtitle: subtitle,
      ));
    }

    Set<String> selected(WidgetTester tester) => tester
        .widget<SegmentedButton<String>>(find.byType(SegmentedButton<String>))
        .selected;

    testWidgets('shows label, options and optional subtitle', (tester) async {
      await tester.pumpWidget(build(changes: [], subtitle: const Text('Sub')));

      expect(find.text('My label'), findsOneWidget);
      expect(find.text('Alpha'), findsOneWidget);
      expect(find.text('Beta'), findsOneWidget);
      expect(find.text('Sub'), findsOneWidget);
    });

    testWidgets('selects nothing when value is null', (tester) async {
      await tester.pumpWidget(build(changes: []));

      expect(selected(tester), isEmpty);
    });

    testWidgets('selects the option matching the value', (tester) async {
      await tester.pumpWidget(build(value: 'b', changes: []));

      expect(selected(tester), {'b'});
    });

    testWidgets('tapping another option reports it', (tester) async {
      final changes = <String>[];
      await tester.pumpWidget(build(value: 'a', changes: changes));

      await tester.tap(find.text('Beta'));
      await tester.pump();

      expect(changes, ['b']);
    });

    testWidgets('tapping the active option does not report', (tester) async {
      final changes = <String>[];
      await tester.pumpWidget(build(value: 'a', changes: changes));

      await tester.tap(find.text('Alpha'));
      await tester.pump();

      expect(changes, isEmpty);
    });

    testWidgets('tapping an option when unset reports it', (tester) async {
      final changes = <String>[];
      await tester.pumpWidget(build(changes: changes));

      await tester.tap(find.text('Alpha'));
      await tester.pump();

      expect(changes, ['a']);
    });
  });
}
