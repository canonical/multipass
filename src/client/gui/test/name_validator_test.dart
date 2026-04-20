import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

void main() {
  Future<AppLocalizations> getL10n(WidgetTester tester) async {
    late AppLocalizations l10n;
    await tester.pumpWidget(
      MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Builder(builder: (context) {
          l10n = AppLocalizations.of(context)!;
          return const SizedBox.shrink();
        }),
      ),
    );
    await tester.pumpAndSettle();
    return l10n;
  }

  group('nameValidator', () {
    testWidgets('empty string is valid (will use random name)', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator(''), isNull);
    });

    testWidgets('single character is invalid (too short)', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('a'), isNotNull);
    });

    testWidgets('name with underscore is invalid (invalid char)',
        (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('my_vm'), isNotNull);
    });

    testWidgets('name with space is invalid (invalid char)', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('my vm'), isNotNull);
    });

    testWidgets('name starting with digit is invalid', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('1vm'), isNotNull);
    });

    testWidgets('name starting with hyphen is invalid', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('-vm'), isNotNull);
    });

    testWidgets('name ending with hyphen is invalid', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('vm-'), isNotNull);
    });

    testWidgets('name in existing names is invalid', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator(['existing-vm', 'other-vm'], [], l10n);
      expect(validator('existing-vm'), isNotNull);
    });

    testWidgets('name in deleted names is invalid', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], ['deleted-vm'], l10n);
      expect(validator('deleted-vm'), isNotNull);
    });

    testWidgets('valid name with hyphen and no conflicts returns null',
        (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('my-vm'), isNull);
    });

    testWidgets('valid name with trailing digit returns null', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('vm1'), isNull);
    });

    testWidgets('valid name with uppercase and digits returns null',
        (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('MyVm1'), isNull);
    });

    testWidgets('exactly 2-character name is valid', (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], [], l10n);
      expect(validator('vm'), isNull);
    });

    testWidgets('error messages are distinct for different validation failures',
        (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator(['taken'], ['deleted'], l10n);

      final tooShort = validator('a');
      final invalidChars = validator('my_vm');
      final noStartLetter = validator('1vm');
      final badEndChar = validator('vm-');
      final inUse = validator('taken');
      final deletedInUse = validator('deleted');

      expect(tooShort, isNotNull);
      expect(invalidChars, isNotNull);
      expect(noStartLetter, isNotNull);
      expect(badEndChar, isNotNull);
      expect(inUse, isNotNull);
      expect(deletedInUse, isNotNull);

      final errors = {
        tooShort,
        invalidChars,
        noStartLetter,
        badEndChar,
        inUse,
        deletedInUse
      };
      expect(errors.length, equals(6));
    });

    testWidgets('name not in existing names is not flagged as in-use',
        (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator(['other-vm'], [], l10n);
      expect(validator('my-vm'), isNull);
    });

    testWidgets('name not in deleted names is not flagged as deleted-in-use',
        (tester) async {
      final l10n = await getL10n(tester);
      final validator = nameValidator([], ['other-vm'], l10n);
      expect(validator('my-vm'), isNull);
    });
  });
}
