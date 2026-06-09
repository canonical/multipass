import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_details/mapping_slider.dart';
import 'package:multipass_gui/vm_details/memory_slider.dart';

Widget buildWidget({int? initialValue, FormFieldSetter<int>? onSaved}) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: Form(
        child: MemorySlider(
          label: 'Memory',
          min: 1.gibi,
          max: 8.gibi,
          sysMax: 4.gibi,
          initialValue: initialValue,
          onSaved: onSaved ?? (_) {},
          // The default formatter goes through FFI, which is unavailable here.
          memoryFormatter: (bytes) => '$bytes bytes',
        ),
      ),
    ),
  );
}

String fieldText(WidgetTester tester) =>
    tester.widget<TextField>(find.byType(TextField)).controller!.text;

void saveForm(WidgetTester tester) =>
    tester.state<FormState>(find.byType(Form)).save();

void main() {
  group('MemorySlider', () {
    testWidgets('labels the range ends with memoryFormatter', (tester) async {
      await tester.pumpWidget(buildWidget());

      expect(find.text('${1.gibi} bytes'), findsOneWidget);
      expect(find.text('${8.gibi} bytes'), findsOneWidget);
    });

    for (final (gibis, warns) in [(5, true), (4, false)]) {
      testWidgets(
          '${warns ? 'warns' : 'does not warn'} about over-provisioning '
          'at $gibis GiB with 4 GiB available', (tester) async {
        await tester.pumpWidget(buildWidget(initialValue: gibis.gibi));

        expect(
          find.byIcon(Icons.warning_rounded),
          warns ? findsOneWidget : findsNothing,
        );
      });
    }

    testWidgets('saves typed values converted from the selected unit',
        (tester) async {
      int? saved;
      await tester.pumpWidget(
        buildWidget(initialValue: 2.gibi, onSaved: (v) => saved = v),
      );

      await tester.enterText(find.byType(TextField), '3');
      saveForm(tester);

      expect(saved, 3.gibi);
    });

    testWidgets('clamps typed values to max when saving', (tester) async {
      int? saved;
      await tester.pumpWidget(
        buildWidget(initialValue: 2.gibi, onSaved: (v) => saved = v),
      );

      await tester.enterText(find.byType(TextField), '16');
      saveForm(tester);

      expect(saved, 8.gibi);
    });

    testWidgets('re-expresses the value when the unit changes', (tester) async {
      await tester.pumpWidget(buildWidget(initialValue: 2.gibi));
      expect(fieldText(tester), '2');

      await tester.tap(find.text('GiB'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('MiB').last);
      await tester.pumpAndSettle();

      expect(fieldText(tester), '2048');
    });

    testWidgets('saves null when there is no value', (tester) async {
      int? saved = 0;
      await tester.pumpWidget(buildWidget(onSaved: (v) => saved = v));

      saveForm(tester);

      expect(saved, isNull);
    });
  });
}
