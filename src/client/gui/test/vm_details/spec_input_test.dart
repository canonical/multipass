import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/spec_input.dart';

Widget buildWidget({
  String? label,
  String? hint,
  String? helper,
  String? initialValue,
  bool enabled = true,
  double width = 170,
  FormFieldSetter<String>? onSaved,
  FormFieldValidator<String>? validator,
  List<TextInputFormatter>? inputFormatters,
}) {
  return MaterialApp(
    home: Scaffold(
      body: Form(
        child: SpecInput(
          label: label,
          hint: hint,
          helper: helper,
          initialValue: initialValue,
          enabled: enabled,
          width: width,
          onSaved: onSaved,
          validator: validator,
          inputFormatters: inputFormatters,
        ),
      ),
    ),
  );
}

void main() {
  group('SpecInput', () {
    testWidgets('renders label text when label is provided', (tester) async {
      await tester.pumpWidget(buildWidget(label: 'Instance Name'));
      expect(find.text('Instance Name'), findsOneWidget);
    });

    testWidgets('does not render label when label is null', (tester) async {
      await tester.pumpWidget(buildWidget());
      expect(find.byType(Text), findsNothing);
    });

    testWidgets('shows hint text in the text field', (tester) async {
      await tester.pumpWidget(buildWidget(hint: 'Enter name'));
      expect(find.text('Enter name'), findsOneWidget);
    });

    testWidgets('shows helper text', (tester) async {
      await tester.pumpWidget(buildWidget(helper: 'Must be unique'));
      expect(find.text('Must be unique'), findsOneWidget);
    });

    testWidgets('populates initial value', (tester) async {
      await tester.pumpWidget(buildWidget(initialValue: 'my-vm'));
      expect(find.text('my-vm'), findsOneWidget);
    });

    testWidgets('applies the given width via SizedBox', (tester) async {
      await tester.pumpWidget(buildWidget(width: 250));
      final sizedBox = tester.widget<SizedBox>(find.byType(SizedBox).first);
      expect(sizedBox.width, equals(250));
    });

    testWidgets('text field is enabled when enabled is true', (tester) async {
      await tester.pumpWidget(buildWidget(enabled: true));
      final field = tester.widget<TextFormField>(find.byType(TextFormField));
      expect(field.enabled, isTrue);
    });

    testWidgets('text field is disabled when enabled is false', (tester) async {
      await tester.pumpWidget(buildWidget(enabled: false));
      final field = tester.widget<TextFormField>(find.byType(TextFormField));
      expect(field.enabled, isFalse);
    });
  });
}
