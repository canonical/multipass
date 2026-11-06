import 'dart:async';

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/copyable_text.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/tooltip.dart';

final _l10n = AppLocalizationsEn();

Widget buildWidget(Widget child) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: Center(child: child)),
  );
}

String tooltipMessage(WidgetTester tester) =>
    tester.widget<Tooltip>(find.byType(Tooltip)).message;

// Records clipboard writes. If [pendingCopy] is given, each write only
// completes once that future does.
List<String> mockClipboard(WidgetTester tester, {Future<void>? pendingCopy}) {
  final copied = <String>[];
  final messenger = tester.binding.defaultBinaryMessenger;
  messenger.setMockMethodCallHandler(SystemChannels.platform, (call) async {
    if (call.method == 'Clipboard.setData') {
      copied.add((call.arguments as Map)['text'] as String);
      await pendingCopy;
    }
    return null;
  });
  addTearDown(
    () => messenger.setMockMethodCallHandler(SystemChannels.platform, null),
  );
  return copied;
}

void main() {
  group('CopyableText', () {
    testWidgets('applies the provided style', (tester) async {
      const style = TextStyle(fontSize: 20);
      await tester.pumpWidget(
        buildWidget(const CopyableText('10.0.0.1', style: style)),
      );

      expect(tester.widget<Text>(find.text('10.0.0.1')).style, style);
    });

    for (final (text, cursor) in [
      ('10.0.0.1', SystemMouseCursors.click),
      ('-', SystemMouseCursors.basic),
    ]) {
      testWidgets('shows a ${cursor.kind} cursor on hover for "$text"',
          (tester) async {
        await tester.pumpWidget(buildWidget(CopyableText(text)));

        final mouse = await tester.createGesture(
          kind: PointerDeviceKind.mouse,
          pointer: 1,
        );
        await mouse.addPointer(location: tester.getCenter(find.text(text)));
        await tester.pump();

        expect(
          tester.binding.mouseTracker.debugDeviceActiveCursor(1),
          cursor,
        );
      });
    }

    testWidgets('tapping copies the text and updates the tooltip',
        (tester) async {
      final copied = mockClipboard(tester);
      await tester.pumpWidget(buildWidget(const CopyableText('10.0.0.1')));
      expect(tooltipMessage(tester), _l10n.copyableTextClickToCopy);

      await tester.tap(find.text('10.0.0.1'));
      await tester.pump();

      expect(copied, ['10.0.0.1']);
      expect(tooltipMessage(tester), _l10n.copyableTextCopied);
    });

    testWidgets('moving the mouse away resets the tooltip', (tester) async {
      mockClipboard(tester);
      await tester.pumpWidget(buildWidget(const CopyableText('10.0.0.1')));

      final mouse = await tester.createGesture(kind: PointerDeviceKind.mouse);
      await mouse.addPointer(location: tester.getCenter(find.text('10.0.0.1')));
      await tester.tap(find.text('10.0.0.1'));
      await tester.pump();
      expect(tooltipMessage(tester), _l10n.copyableTextCopied);

      await mouse.removePointer();
      await tester.pump();

      expect(tooltipMessage(tester), _l10n.copyableTextClickToCopy);
    });

    testWidgets('"-" placeholder has no tooltip and is not copied on tap',
        (tester) async {
      final copied = mockClipboard(tester);
      await tester.pumpWidget(buildWidget(const CopyableText('-')));

      await tester.tap(find.text('-'));
      await tester.pump();

      expect(find.byType(Tooltip), findsNothing);
      expect(copied, isEmpty);
    });

    testWidgets('does not throw when removed while the copy is in progress',
        (tester) async {
      final copyDone = Completer<void>();
      mockClipboard(tester, pendingCopy: copyDone.future);
      await tester.pumpWidget(buildWidget(const CopyableText('10.0.0.1')));

      await tester.tap(find.text('10.0.0.1'));
      await tester.pumpWidget(buildWidget(const SizedBox()));
      copyDone.complete();
      await tester.pump();

      expect(tester.takeException(), isNull);
    });
  });
}
