import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/settings/usage_settings.dart';

Widget _buildApp(Widget child) {
  return MaterialApp(
    locale: const Locale('en'),
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: child),
  );
}

void main() {
  group('PrimaryNameField validation', () {
    Future<void> pumpField(
      WidgetTester tester, {
      String value = '',
      void Function(String)? onSave,
    }) async {
      await tester.pumpWidget(
        _buildApp(
          Builder(
            builder: (context) {
              final l10n = AppLocalizations.of(context)!;
              return PrimaryNameField(
                value: value,
                l10n: l10n,
                onSave: onSave ?? (_) {},
              );
            },
          ),
        ),
      );
      await tester.pumpAndSettle();
    }

    testWidgets('empty input shows no validation error', (tester) async {
      await pumpField(tester, value: '');

      await tester.enterText(find.byType(TextFormField), '');
      await tester.pump();

      expect(find.byType(ErrorWidget), findsNothing);
    });

    testWidgets('input starting with a digit shows error', (tester) async {
      await pumpField(tester, value: '');

      await tester.enterText(find.byType(TextFormField), '1abc');
      await tester.pump();

      expect(find.byIcon(Icons.check), findsOneWidget);
      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();

      final l10n = tester.element(find.byType(TextFormField)).l10n;
      expect(find.text(l10n.usagePrimaryNameErrorStartLetter), findsOneWidget);
    });

    testWidgets('single character input shows too-short error', (tester) async {
      await pumpField(tester, value: '');

      await tester.enterText(find.byType(TextFormField), 'a');
      await tester.pump();
      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();

      final l10n = tester.element(find.byType(TextFormField)).l10n;
      expect(find.text(l10n.usagePrimaryNameErrorTooShort), findsOneWidget);
    });

    testWidgets('input ending with a dash shows error', (tester) async {
      await pumpField(tester, value: '');

      await tester.enterText(find.byType(TextFormField), 'abc-');
      await tester.pump();
      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();

      final l10n = tester.element(find.byType(TextFormField)).l10n;
      expect(find.text(l10n.usagePrimaryNameErrorEndChar), findsOneWidget);
    });

    testWidgets('valid input calls onSave', (tester) async {
      String? saved;
      await pumpField(tester, value: '', onSave: (v) => saved = v);

      await tester.enterText(find.byType(TextFormField), 'my-vm');
      await tester.pump();
      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();

      expect(saved, equals('my-vm'));
    });

    testWidgets('discard reverts text to original value', (tester) async {
      await pumpField(tester, value: 'original');

      await tester.enterText(find.byType(TextFormField), 'changed');
      await tester.pump();

      expect(find.byIcon(Icons.close), findsOneWidget);
      await tester.tap(find.byIcon(Icons.close));
      await tester.pump();

      final textField =
          tester.widget<TextFormField>(find.byType(TextFormField));
      expect(textField.controller?.text, equals('original'));
    });
  });

  group('SettingField', () {
    testWidgets('save and discard buttons hidden when not changed',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(
          SettingField(
            label: 'Test Label',
            onSave: () {},
            onDiscard: () {},
            changed: false,
            child: const SizedBox(),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.byIcon(Icons.check), findsNothing);
      expect(find.byIcon(Icons.close), findsNothing);
    });

    testWidgets('save and discard buttons visible when changed',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(
          SettingField(
            label: 'Test Label',
            onSave: () {},
            onDiscard: () {},
            changed: true,
            child: const SizedBox(),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.byIcon(Icons.check), findsOneWidget);
      expect(find.byIcon(Icons.close), findsOneWidget);
    });

    testWidgets('tapping save calls onSave', (tester) async {
      var saveCalled = false;
      await tester.pumpWidget(
        _buildApp(
          SettingField(
            label: 'Test Label',
            onSave: () => saveCalled = true,
            onDiscard: () {},
            changed: true,
            child: const SizedBox(),
          ),
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.byIcon(Icons.check));
      await tester.pump();

      expect(saveCalled, isTrue);
    });

    testWidgets('tapping discard calls onDiscard', (tester) async {
      var discardCalled = false;
      await tester.pumpWidget(
        _buildApp(
          SettingField(
            label: 'Test Label',
            onSave: () {},
            onDiscard: () => discardCalled = true,
            changed: true,
            child: const SizedBox(),
          ),
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.byIcon(Icons.close));
      await tester.pump();

      expect(discardCalled, isTrue);
    });

    testWidgets('label text is displayed', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          SettingField(
            label: 'My Setting',
            onSave: () {},
            onDiscard: () {},
            changed: false,
            child: const SizedBox(),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('My Setting'), findsOneWidget);
    });
  });
}

extension on BuildContext {
  AppLocalizations get l10n => AppLocalizations.of(this)!;
}
