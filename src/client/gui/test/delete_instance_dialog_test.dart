import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/confirmation_dialog.dart';
import 'package:multipass_gui/delete_instance_dialog.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

Widget buildWidget({required VoidCallback onDelete, int count = 1}) {
  return MaterialApp(
    locale: const Locale('en'),
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
    testWidgets('renders a ConfirmationDialog', (tester) async {
      await tester.pumpWidget(buildWidget(onDelete: () {}));
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();

      expect(find.byType(ConfirmationDialog), findsOneWidget);
    });

    testWidgets('invoking delete calls onDelete and closes the dialog',
        (tester) async {
      var deleted = false;
      await tester.pumpWidget(buildWidget(onDelete: () => deleted = true));
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();

      // The delete button has an explicit red backgroundColor style.
      await tester.tap(find.byWidgetPredicate(
        (w) => w is TextButton && w.style?.backgroundColor != null,
      ));
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

      await tester.tap(find.byType(OutlinedButton));
      await tester.pumpAndSettle();

      expect(deleted, isFalse);
      expect(find.byType(ConfirmationDialog), findsNothing);
    });
  });
}
