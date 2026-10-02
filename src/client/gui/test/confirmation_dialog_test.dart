import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/confirmation_dialog.dart';

void main() {
  Widget buildDialog({
    required String title,
    required Widget body,
    required String actionText,
    required VoidCallback onAction,
    required String inactionText,
    required VoidCallback onInaction,
  }) {
    return MaterialApp(
      home: Scaffold(
        body: Dialog(
          child: ConfirmationDialog(
            title: title,
            body: body,
            actionText: actionText,
            onAction: onAction,
            inactionText: inactionText,
            onInaction: onInaction,
          ),
        ),
      ),
    );
  }

  group('ConfirmationDialog', () {
    testWidgets('tapping action button invokes onAction', (tester) async {
      var actionCalled = false;

      await tester.pumpWidget(buildDialog(
        title: 'Delete instance',
        body: const Text('Are you sure?'),
        actionText: 'Delete',
        onAction: () => actionCalled = true,
        inactionText: 'Cancel',
        onInaction: () {},
      ));

      await tester.tap(find.text('Delete'));
      await tester.pumpAndSettle();

      expect(actionCalled, isTrue);
    });

    testWidgets('tapping inaction button invokes onInaction', (tester) async {
      var inactionCalled = false;

      await tester.pumpWidget(buildDialog(
        title: 'Delete instance',
        body: const Text('Are you sure?'),
        actionText: 'Delete',
        onAction: () {},
        inactionText: 'Cancel',
        onInaction: () => inactionCalled = true,
      ));

      await tester.tap(find.text('Cancel'));
      await tester.pumpAndSettle();

      expect(inactionCalled, isTrue);
    });

    testWidgets('tapping close icon button dismisses the dialog',
        (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: Builder(
            builder: (context) => Scaffold(
              body: TextButton(
                onPressed: () => showDialog<void>(
                  context: context,
                  builder: (_) => ConfirmationDialog(
                    title: 'Delete instance',
                    body: const Text('Are you sure?'),
                    actionText: 'Delete',
                    onAction: () {},
                    inactionText: 'Cancel',
                    onInaction: () {},
                  ),
                ),
                child: const Text('Open'),
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();

      expect(find.text('Delete instance'), findsOneWidget);

      await tester.tap(find.byIcon(Icons.close));
      await tester.pumpAndSettle();

      expect(find.text('Delete instance'), findsNothing);
    });

    testWidgets('tapping action button does not invoke onInaction',
        (tester) async {
      var inactionCalled = false;

      await tester.pumpWidget(buildDialog(
        title: 'Delete instance',
        body: const Text('Are you sure?'),
        actionText: 'Delete',
        onAction: () {},
        inactionText: 'Cancel',
        onInaction: () => inactionCalled = true,
      ));

      await tester.tap(find.text('Delete'));
      await tester.pumpAndSettle();

      expect(inactionCalled, isFalse);
    });

    testWidgets('tapping inaction button does not invoke onAction',
        (tester) async {
      var actionCalled = false;

      await tester.pumpWidget(buildDialog(
        title: 'Delete instance',
        body: const Text('Are you sure?'),
        actionText: 'Delete',
        onAction: () => actionCalled = true,
        inactionText: 'Cancel',
        onInaction: () {},
      ));

      await tester.tap(find.text('Cancel'));
      await tester.pumpAndSettle();

      expect(actionCalled, isFalse);
    });
  });
}
