import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/cpus_slider.dart';

Widget buildWidget({
  required int cores,
  int? initialValue,
  FormFieldSetter<int>? onSaved,
}) {
  return ProviderScope(
    overrides: [
      daemonInfoProvider.overrideWith(
        (_) async => DaemonInfoReply(cpus: cores),
      ),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: Form(
          child: CpusSlider(
            initialValue: initialValue,
            onSaved: onSaved ?? (_) {},
          ),
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
  group('CpusSlider', () {
    testWidgets('labels the range from 1 to the number of cores',
        (tester) async {
      await tester.pumpWidget(buildWidget(cores: 6, initialValue: 2));
      await tester.pumpAndSettle();

      expect(find.text('1'), findsOneWidget);
      expect(find.text('6'), findsOneWidget);
    });

    for (final (value, warns) in [(5, true), (4, false)]) {
      testWidgets(
          '${warns ? 'warns' : 'does not warn'} about over-provisioning '
          'at $value CPUs with 4 cores', (tester) async {
        await tester.pumpWidget(buildWidget(cores: 4, initialValue: value));
        await tester.pumpAndSettle();

        expect(
          find.byIcon(Icons.warning_rounded),
          warns ? findsOneWidget : findsNothing,
        );
      });
    }

    testWidgets('rejects non-integer input', (tester) async {
      await tester.pumpWidget(buildWidget(cores: 4, initialValue: 2));
      await tester.pumpAndSettle();

      await tester.enterText(find.byType(TextField), 'abc');

      expect(fieldText(tester), '2');
    });

    testWidgets('saves the initial value when untouched', (tester) async {
      int? saved;
      await tester.pumpWidget(
        buildWidget(cores: 8, initialValue: 3, onSaved: (v) => saved = v),
      );
      await tester.pumpAndSettle();

      saveForm(tester);

      expect(saved, 3);
    });

    for (final (input, expected) in [('3', 3), ('99', 4), ('0', 1)]) {
      testWidgets('saves typed "$input" as $expected with 4 cores',
          (tester) async {
        int? saved;
        await tester.pumpWidget(
          buildWidget(cores: 4, initialValue: 2, onSaved: (v) => saved = v),
        );
        await tester.pumpAndSettle();

        await tester.enterText(find.byType(TextField), input);
        saveForm(tester);

        expect(saved, expected);
      });
    }

    testWidgets('snaps the field to the clamped value when focus is lost',
        (tester) async {
      await tester.pumpWidget(buildWidget(cores: 4, initialValue: 2));
      await tester.pumpAndSettle();

      await tester.enterText(find.byType(TextField), '99');
      FocusManager.instance.primaryFocus!.unfocus();
      await tester.pump();

      expect(fieldText(tester), '4');
    });
  });
}
