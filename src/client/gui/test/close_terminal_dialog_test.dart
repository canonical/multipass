import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/close_terminal_dialog.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';

final _l10n = AppLocalizationsEn();

Future<void> pumpDialog(
  WidgetTester tester, {
  VoidCallback? onYes,
  VoidCallback? onNo,
  ValueChanged<bool>? onDoNotAsk,
}) async {
  await tester.pumpWidget(MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: Builder(
        builder: (context) => TextButton(
          onPressed: () => showDialog(
            context: context,
            builder: (_) => CloseTerminalDialog(
              onYes: onYes ?? () {},
              onNo: onNo ?? () {},
              onDoNotAsk: onDoNotAsk ?? (_) {},
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
  group('CloseTerminalDialog', () {
    for (final (taps, doNotAsk) in [(0, false), (1, true), (2, false)]) {
      testWidgets(
          'confirm button calls onYes and onDoNotAsk($doNotAsk) '
          'after $taps checkbox taps', (tester) async {
        var closed = false;
        bool? savedDoNotAsk;
        await pumpDialog(
          tester,
          onYes: () => closed = true,
          onDoNotAsk: (v) => savedDoNotAsk = v,
        );

        await tapCheckbox(tester, taps);
        await tester.tap(find.text(_l10n.closeTerminalConfirm));
        await tester.pump();

        expect(closed, isTrue);
        expect(savedDoNotAsk, doNotAsk);
      });
    }

    testWidgets('cancel button calls onNo without saving doNotAsk',
        (tester) async {
      var cancelled = false;
      var doNotAskSaved = false;
      await pumpDialog(
        tester,
        onNo: () => cancelled = true,
        onDoNotAsk: (_) => doNotAskSaved = true,
      );

      await tapCheckbox(tester, 1);
      await tester.tap(find.text(_l10n.commonCancel));
      await tester.pump();

      expect(cancelled, isTrue);
      expect(doNotAskSaved, isFalse);
    });

    testWidgets('close button dismisses without calling any callback',
        (tester) async {
      var called = false;
      await pumpDialog(
        tester,
        onYes: () => called = true,
        onNo: () => called = true,
        onDoNotAsk: (_) => called = true,
      );

      await tapCheckbox(tester, 1);
      await tester.tap(find.byIcon(Icons.close));
      await tester.pumpAndSettle();

      expect(called, isFalse);
      expect(find.byType(CloseTerminalDialog), findsNothing);
    });
  });
}
