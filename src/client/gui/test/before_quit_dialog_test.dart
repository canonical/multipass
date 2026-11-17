import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/before_quit_dialog.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';

final _l10n = AppLocalizationsEn();

Future<void> pumpDialog(
  WidgetTester tester, {
  int runningCount = 1,
  ValueChanged<bool>? onStop,
  ValueChanged<bool>? onKeep,
}) async {
  await tester.pumpWidget(MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: Builder(
        builder: (context) => TextButton(
          onPressed: () => showDialog(
            context: context,
            builder: (_) => BeforeQuitDialog(
              runningCount: runningCount,
              onStop: onStop ?? (_) {},
              onKeep: onKeep ?? (_) {},
            ),
          ),
          child: const Text('Open'),
        ),
      ),
    ),
  ));
  await tester.tap(find.text('Open'));
  await tester.pumpAndSettle();
}

Future<void> tapCheckbox(WidgetTester tester, int times) async {
  for (var i = 0; i < times; i++) {
    await tester.tap(find.byType(Checkbox));
    await tester.pump();
  }
}

void main() {
  group('BeforeQuitDialog', () {
    for (final count in [1, 3, 10]) {
      testWidgets('shows the message for $count running instances',
          (tester) async {
        await pumpDialog(tester, runningCount: count);

        expect(find.text(_l10n.beforeQuitTitle), findsOneWidget);
        expect(
          find.text(
            _l10n.beforeQuitMessage(count, _l10n.spellOutNumber('$count')),
          ),
          findsOneWidget,
        );
      });
    }

    for (final (button, callback) in [
      (_l10n.beforeQuitStopAction, 'onStop'),
      (_l10n.beforeQuitKeepAction, 'onKeep'),
    ]) {
      for (final (taps, remember) in [(0, false), (1, true), (2, false)]) {
        testWidgets(
            '"$button" calls only $callback with remember=$remember '
            'after $taps checkbox taps', (tester) async {
          final calls = <(String, bool)>[];
          await pumpDialog(
            tester,
            onStop: (v) => calls.add(('onStop', v)),
            onKeep: (v) => calls.add(('onKeep', v)),
          );

          await tapCheckbox(tester, taps);
          await tester.tap(find.text(button));
          await tester.pump();

          expect(calls, [(callback, remember)]);
        });
      }
    }

    testWidgets('close button dismisses without calling onStop or onKeep',
        (tester) async {
      final calls = <(String, bool)>[];
      await pumpDialog(
        tester,
        onStop: (v) => calls.add(('onStop', v)),
        onKeep: (v) => calls.add(('onKeep', v)),
      );

      await tapCheckbox(tester, 1);
      await tester.tap(find.byIcon(Icons.close));
      await tester.pumpAndSettle();

      expect(calls, isEmpty);
      expect(find.byType(BeforeQuitDialog), findsNothing);
    });
  });
}
