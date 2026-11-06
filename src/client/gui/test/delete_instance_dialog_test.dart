import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/confirmation_dialog.dart';
import 'package:multipass_gui/delete_instance_dialog.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';

final _l10n = AppLocalizationsEn();

Widget buildWidget({required VoidCallback onDelete, int count = 1}) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: Builder(
        builder: (context) => TextButton(
          onPressed: () => showDialog(
            context: context,
            builder: (_) => DeleteInstanceDialog(
              onDelete: onDelete,
              count: count,
            ),
          ),
          child: const Text('Open'),
        ),
      ),
    ),
  );
}

void main() {
  group('DeleteInstanceDialog', () {
    for (final count in [1, 2]) {
      testWidgets('uses count=$count for the title and body', (tester) async {
        await tester.pumpWidget(buildWidget(onDelete: () {}, count: count));
        await tester.tap(find.text('Open'));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.deleteInstanceTitle(count)), findsOneWidget);
        expect(find.text(_l10n.deleteInstanceBody(count)), findsOneWidget);
      });
    }

    testWidgets('delete button calls onDelete and closes the dialog',
        (tester) async {
      var deleted = false;
      await tester.pumpWidget(buildWidget(onDelete: () => deleted = true));
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.commonDelete));
      await tester.pumpAndSettle();

      expect(deleted, isTrue);
      expect(find.byType(ConfirmationDialog), findsNothing);
    });

    testWidgets('cancel button closes the dialog without invoking onDelete',
        (tester) async {
      var deleted = false;
      await tester.pumpWidget(buildWidget(onDelete: () => deleted = true));
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.commonCancel));
      await tester.pumpAndSettle();

      expect(deleted, isFalse);
      expect(find.byType(ConfirmationDialog), findsNothing);
    });
  });
}
