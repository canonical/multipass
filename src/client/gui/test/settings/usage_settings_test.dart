import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/settings/hotkey.dart';
import 'package:multipass_gui/settings/usage_settings.dart';

final _l10n = AppLocalizationsEn();

Widget _buildApp(Widget child) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: child),
  );
}

void main() {
  group('PrimaryNameField', () {
    Future<void> pumpField(
      WidgetTester tester, {
      String value = '',
      ValueChanged<String>? onSave,
    }) async {
      await tester.pumpWidget(
        _buildApp(
          PrimaryNameField(
            value: value,
            l10n: _l10n,
            onSave: onSave ?? (_) {},
          ),
        ),
      );
    }

    Future<void> enterAndSave(WidgetTester tester, String text) async {
      await tester.enterText(find.byType(TextFormField), text);
      await tester.pump();
      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();
    }

    String fieldText(WidgetTester tester) => tester
        .widget<TextFormField>(find.byType(TextFormField))
        .controller!
        .text;

    for (final (input, error) in [
      ('1abc', _l10n.usagePrimaryNameErrorStartLetter),
      ('a', _l10n.usagePrimaryNameErrorTooShort),
      ('abc-', _l10n.usagePrimaryNameErrorEndChar),
    ]) {
      testWidgets('rejects "$input" without saving', (tester) async {
        String? saved;
        await pumpField(tester, onSave: (v) => saved = v);

        await enterAndSave(tester, input);

        expect(find.text(error), findsOneWidget);
        expect(saved, isNull);
      });
    }

    for (final input in ['my-vm', 'ab', 'A1', 'a-b']) {
      testWidgets('saves "$input"', (tester) async {
        String? saved;
        await pumpField(tester, onSave: (v) => saved = v);

        await enterAndSave(tester, input);

        expect(saved, input);
      });
    }

    testWidgets('saves an empty name to unset the primary instance',
        (tester) async {
      String? saved;
      await pumpField(tester, value: 'primary', onSave: (v) => saved = v);

      await enterAndSave(tester, '');

      expect(saved, isEmpty);
    });

    testWidgets('drops characters other than letters, digits and dashes',
        (tester) async {
      await pumpField(tester);

      await tester.enterText(find.byType(TextFormField), 'my_vm.1!');

      expect(fieldText(tester), 'myvm1');
    });

    testWidgets('discard restores the original value and clears the error',
        (tester) async {
      await pumpField(tester, value: 'original');
      await enterAndSave(tester, '1abc');

      await tester.tap(find.byIcon(Icons.close));
      await tester.pump();

      expect(fieldText(tester), 'original');
      expect(find.text(_l10n.usagePrimaryNameErrorStartLetter), findsNothing);
      expect(find.byIcon(Icons.check), findsNothing);
    });

    testWidgets('keeps unsaved edits when the parent rebuilds', (tester) async {
      await pumpField(tester, value: 'original');
      await tester.enterText(find.byType(TextFormField), 'edited');
      await tester.pump();

      await pumpField(tester, value: 'original');

      expect(fieldText(tester), 'edited');
      expect(find.byIcon(Icons.check), findsOneWidget);
    });

    testWidgets('shows the new value when it changes externally',
        (tester) async {
      await pumpField(tester, value: 'old');

      await pumpField(tester, value: 'new');

      expect(fieldText(tester), 'new');
      expect(find.byIcon(Icons.check), findsNothing);
    });
  });

  group('HotkeyField', () {
    const original = SingleActivator(LogicalKeyboardKey.keyT, control: true);

    Future<void> pumpField(
      WidgetTester tester, {
      ValueChanged<SingleActivator?>? onSave,
    }) async {
      await tester.pumpWidget(
        _buildApp(
          HotkeyField(value: original, l10n: _l10n, onSave: onSave ?? (_) {}),
        ),
      );
    }

    Future<void> recordMetaK(WidgetTester tester) async {
      await tester.tap(find.byType(HotkeyRecorder));
      await tester.pump();
      await tester.sendKeyDownEvent(LogicalKeyboardKey.metaLeft);
      await tester.sendKeyDownEvent(LogicalKeyboardKey.keyK);
      await tester.pump();
      await tester.sendKeyUpEvent(LogicalKeyboardKey.keyK);
      await tester.sendKeyUpEvent(LogicalKeyboardKey.metaLeft);
      await tester.pump();
    }

    testWidgets('saves the recorded hotkey', (tester) async {
      SingleActivator? saved;
      await pumpField(tester, onSave: (v) => saved = v);
      await recordMetaK(tester);

      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();

      expect(saved?.trigger, LogicalKeyboardKey.keyK);
      expect(saved?.meta, isTrue);
    });

    testWidgets('discarded hotkey does not reappear when the parent rebuilds',
        (tester) async {
      await pumpField(tester);
      await recordMetaK(tester);

      await tester.tap(find.byIcon(Icons.close));
      await tester.pump();
      expect(find.byIcon(Icons.check), findsNothing);

      await pumpField(tester);

      expect(find.byIcon(Icons.check), findsNothing);
    });
  });

  group('PassphraseField', () {
    Future<void> pumpField(
      WidgetTester tester, {
      ValueChanged<String>? onSave,
    }) async {
      await tester.pumpWidget(
        _buildApp(
          PassphraseField(
            hasPassphrase: false,
            l10n: _l10n,
            onSave: onSave ?? (_) {},
          ),
        ),
      );
    }

    testWidgets('saves the passphrase and clears the field', (tester) async {
      String? saved;
      await pumpField(tester, onSave: (v) => saved = v);
      await tester.enterText(find.byType(TextField), 'secret');
      await tester.pump(PassphraseField.changeDelay);

      await tester.tap(find.byIcon(Icons.check));
      await tester.pump(PassphraseField.changeDelay);

      expect(saved, 'secret');
      expect(
        tester.widget<TextField>(find.byType(TextField)).controller!.text,
        isEmpty,
      );
    });

    testWidgets('does not update state after being disposed', (tester) async {
      await pumpField(tester);
      await tester.enterText(find.byType(TextField), 'secret');

      await tester.pumpWidget(_buildApp(const SizedBox()));
      await tester.pump(PassphraseField.changeDelay);

      expect(tester.takeException(), isNull);
    });
  });

  group('SettingField', () {
    Future<void> pumpField(
      WidgetTester tester, {
      required bool changed,
      VoidCallback? onSave,
      VoidCallback? onDiscard,
    }) async {
      await tester.pumpWidget(
        _buildApp(
          SettingField(
            label: 'Label',
            onSave: onSave ?? () {},
            onDiscard: onDiscard ?? () {},
            changed: changed,
            child: const SizedBox(),
          ),
        ),
      );
    }

    testWidgets('hides save and discard buttons when unchanged',
        (tester) async {
      await pumpField(tester, changed: false);

      expect(find.byIcon(Icons.check), findsNothing);
      expect(find.byIcon(Icons.close), findsNothing);
    });

    testWidgets('save and discard buttons invoke their callbacks',
        (tester) async {
      var saved = false;
      var discarded = false;
      await pumpField(
        tester,
        changed: true,
        onSave: () => saved = true,
        onDiscard: () => discarded = true,
      );

      await tester.tap(find.byIcon(Icons.check));
      expect((saved, discarded), (true, false));

      await tester.tap(find.byIcon(Icons.close));
      expect((saved, discarded), (true, true));
    });
  });
}
