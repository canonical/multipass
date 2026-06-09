import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/cpus_slider.dart';

Widget _buildSlider(
    {required int cpus, int? initialValue, void Function(int?)? onSaved}) {
  return ProviderScope(
    overrides: [
      daemonInfoProvider.overrideWith((_) async => DaemonInfoReply(cpus: cpus)),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: SizedBox(
          width: 600,
          child: Form(
            child: CpusSlider(
              initialValue: initialValue,
              onSaved: onSaved ?? (_) {},
            ),
          ),
        ),
      ),
    ),
  );
}

void main() {
  group('CpusSlider', () {
    testWidgets('initial value shown in text field', (tester) async {
      await tester.pumpWidget(_buildSlider(cpus: 8, initialValue: 3));
      await tester.pumpAndSettle();

      final textField = tester.widget<TextField>(find.byType(TextField).first);
      expect(textField.controller?.text, equals('3'));
    });

    testWidgets('text field rejects non-integer input', (tester) async {
      await tester.pumpWidget(_buildSlider(cpus: 4, initialValue: 2));
      await tester.pumpAndSettle();

      await tester.enterText(find.byType(TextField).first, 'abc');
      await tester.pump();

      final textField = tester.widget<TextField>(find.byType(TextField).first);
      // 'abc' should be rejected by the input formatter; the field stays at '2'
      expect(textField.controller?.text, equals('2'));
    });

    testWidgets('over-provisioning warning shown when value exceeds cores',
        (tester) async {
      await tester.pumpWidget(_buildSlider(cpus: 2, initialValue: 4));
      await tester.pumpAndSettle();

      expect(find.byIcon(Icons.warning_rounded), findsOneWidget);
    });

    testWidgets('no over-provisioning warning when value equals cores',
        (tester) async {
      await tester.pumpWidget(_buildSlider(cpus: 4, initialValue: 4));
      await tester.pumpAndSettle();

      expect(find.byIcon(Icons.warning_rounded), findsNothing);
    });

    testWidgets('onSaved is called with current value when form is saved',
        (tester) async {
      int? savedValue;
      final formKey = GlobalKey<FormState>();

      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            daemonInfoProvider.overrideWith(
              (_) async => DaemonInfoReply(cpus: 8),
            ),
          ],
          child: MaterialApp(
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(
              body: SizedBox(
                width: 600,
                child: Form(
                  key: formKey,
                  child: CpusSlider(
                    initialValue: 3,
                    onSaved: (v) => savedValue = v,
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();

      formKey.currentState!.save();
      await tester.pump();

      expect(savedValue, equals(3));
    });

    testWidgets('min and max range labels are displayed', (tester) async {
      await tester.pumpWidget(_buildSlider(cpus: 6, initialValue: 2));
      await tester.pumpAndSettle();

      expect(find.text('1'), findsOneWidget);
      expect(find.text('6'), findsOneWidget);
    });
  });
}
