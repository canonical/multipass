import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/spec_input.dart';

Widget buildWidget(SpecInput input) {
  return MaterialApp(home: Scaffold(body: Form(child: input)));
}

FormState formState(WidgetTester tester) =>
    tester.state<FormState>(find.byType(Form));

void main() {
  group('SpecInput', () {
    testWidgets('shows the label when provided', (tester) async {
      await tester.pumpWidget(buildWidget(const SpecInput(label: 'Name')));

      expect(find.text('Name'), findsOneWidget);
    });

    testWidgets('omits the label when null', (tester) async {
      await tester.pumpWidget(buildWidget(const SpecInput()));

      expect(find.byType(Text), findsNothing);
    });

    testWidgets('shows hint and helper text', (tester) async {
      await tester.pumpWidget(
        buildWidget(const SpecInput(hint: 'Enter name', helper: 'Be unique')),
      );

      expect(find.text('Enter name'), findsOneWidget);
      expect(find.text('Be unique'), findsOneWidget);
    });

    testWidgets('takes the given width', (tester) async {
      await tester.pumpWidget(buildWidget(const SpecInput(width: 250)));

      expect(tester.getSize(find.byType(SpecInput)).width, 250);
    });

    testWidgets('saves the initial value when untouched', (tester) async {
      String? saved;
      await tester.pumpWidget(buildWidget(
        SpecInput(initialValue: 'my-vm', onSaved: (v) => saved = v),
      ));

      formState(tester).save();

      expect(saved, 'my-vm');
    });

    testWidgets('saves the entered text', (tester) async {
      String? saved;
      await tester.pumpWidget(buildWidget(
        SpecInput(initialValue: 'my-vm', onSaved: (v) => saved = v),
      ));

      await tester.enterText(find.byType(TextFormField), 'other-vm');
      formState(tester).save();

      expect(saved, 'other-vm');
    });

    testWidgets('shows the validator error', (tester) async {
      await tester.pumpWidget(buildWidget(
        SpecInput(validator: (v) => v!.isEmpty ? 'Required' : null),
      ));

      formState(tester).validate();
      await tester.pump();

      expect(find.text('Required'), findsOneWidget);
    });

    testWidgets('applies input formatters', (tester) async {
      await tester.pumpWidget(buildWidget(
        SpecInput(inputFormatters: [FilteringTextInputFormatter.digitsOnly]),
      ));

      await tester.enterText(find.byType(TextFormField), 'a1b2');

      expect(find.text('12'), findsOneWidget);
    });

    testWidgets('disables the field when enabled is false', (tester) async {
      await tester.pumpWidget(buildWidget(const SpecInput(enabled: false)));

      expect(tester.widget<TextField>(find.byType(TextField)).enabled, isFalse);
    });
  });
}
