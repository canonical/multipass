import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/before_quit_dialog.dart';
import 'package:multipass_gui/close_terminal_dialog.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

Widget buildLocalized(Widget widget) {
  return MaterialApp(
    locale: const Locale('en'),
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: Dialog(child: widget)),
  );
}

void main() {
  group('BeforeQuitDialog', () {
    testWidgets('initial remember state is false', (tester) async {
      await tester.pumpWidget(buildLocalized(
        BeforeQuitDialog(
          runningCount: 1,
          onStop: (_) {},
          onKeep: (_) {},
        ),
      ));
      await tester.pumpAndSettle();

      expect(tester.widget<Checkbox>(find.byType(Checkbox)).value, isFalse);
    });

    testWidgets('tapping Checkbox toggles remember to true', (tester) async {
      await tester.pumpWidget(buildLocalized(
        BeforeQuitDialog(
          runningCount: 1,
          onStop: (_) {},
          onKeep: (_) {},
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(Checkbox));
      await tester.pump();

      expect(tester.widget<Checkbox>(find.byType(Checkbox)).value, isTrue);
    });

    testWidgets('tapping action button calls onStop(false) when remember=false',
        (tester) async {
      bool? rememberedValue;
      await tester.pumpWidget(buildLocalized(
        BeforeQuitDialog(
          runningCount: 1,
          onStop: (v) => rememberedValue = v,
          onKeep: (_) {},
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(TextButton));
      await tester.pump();

      expect(rememberedValue, isFalse);
    });

    testWidgets(
        'tapping action button calls onStop(true) when checkbox checked first',
        (tester) async {
      bool? rememberedValue;
      await tester.pumpWidget(buildLocalized(
        BeforeQuitDialog(
          runningCount: 1,
          onStop: (v) => rememberedValue = v,
          onKeep: (_) {},
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(Checkbox));
      await tester.pump();
      await tester.tap(find.byType(TextButton));
      await tester.pump();

      expect(rememberedValue, isTrue);
    });

    testWidgets(
        'tapping inaction button calls onKeep(false) when remember=false',
        (tester) async {
      bool? keptValue;
      await tester.pumpWidget(buildLocalized(
        BeforeQuitDialog(
          runningCount: 1,
          onStop: (_) {},
          onKeep: (v) => keptValue = v,
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(OutlinedButton));
      await tester.pump();

      expect(keptValue, isFalse);
    });

    testWidgets(
        'tapping inaction button calls onKeep(true) when checkbox checked first',
        (tester) async {
      bool? keptValue;
      await tester.pumpWidget(buildLocalized(
        BeforeQuitDialog(
          runningCount: 1,
          onStop: (_) {},
          onKeep: (v) => keptValue = v,
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(Checkbox));
      await tester.pump();
      await tester.tap(find.byType(OutlinedButton));
      await tester.pump();

      expect(keptValue, isTrue);
    });
  });

  group('CloseTerminalDialog', () {
    testWidgets('initial doNotAsk state is false', (tester) async {
      await tester.pumpWidget(buildLocalized(
        CloseTerminalDialog(
          onYes: () {},
          onNo: () {},
          onDoNotAsk: (_) {},
        ),
      ));
      await tester.pumpAndSettle();

      expect(tester.widget<Checkbox>(find.byType(Checkbox)).value, isFalse);
    });

    testWidgets('tapping Checkbox toggles doNotAsk to true', (tester) async {
      await tester.pumpWidget(buildLocalized(
        CloseTerminalDialog(
          onYes: () {},
          onNo: () {},
          onDoNotAsk: (_) {},
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(Checkbox));
      await tester.pump();

      expect(tester.widget<Checkbox>(find.byType(Checkbox)).value, isTrue);
    });

    testWidgets(
        'tapping action button calls onYes() and onDoNotAsk(false) when doNotAsk=false',
        (tester) async {
      bool yesCalled = false;
      bool? doNotAskValue;
      await tester.pumpWidget(buildLocalized(
        CloseTerminalDialog(
          onYes: () => yesCalled = true,
          onNo: () {},
          onDoNotAsk: (v) => doNotAskValue = v,
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(TextButton));
      await tester.pump();

      expect(yesCalled, isTrue);
      expect(doNotAskValue, isFalse);
    });

    testWidgets(
        'tapping action button calls onDoNotAsk(true) and onYes() when checkbox checked first',
        (tester) async {
      bool yesCalled = false;
      bool? doNotAskValue;
      await tester.pumpWidget(buildLocalized(
        CloseTerminalDialog(
          onYes: () => yesCalled = true,
          onNo: () {},
          onDoNotAsk: (v) => doNotAskValue = v,
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(Checkbox));
      await tester.pump();
      await tester.tap(find.byType(TextButton));
      await tester.pump();

      expect(doNotAskValue, isTrue);
      expect(yesCalled, isTrue);
    });

    testWidgets(
        'tapping inaction button calls onNo() and does NOT call onDoNotAsk',
        (tester) async {
      bool noCalled = false;
      bool doNotAskCalled = false;
      await tester.pumpWidget(buildLocalized(
        CloseTerminalDialog(
          onYes: () {},
          onNo: () => noCalled = true,
          onDoNotAsk: (_) => doNotAskCalled = true,
        ),
      ));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(OutlinedButton));
      await tester.pump();

      expect(noCalled, isTrue);
      expect(doNotAskCalled, isFalse);
    });
  });
}
