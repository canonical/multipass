import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/confirmation_dialog.dart';

void main() {
  Future<void> openDialog(
    WidgetTester tester, {
    VoidCallback? onAction,
    VoidCallback? onInaction,
  }) async {
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
                  onAction: onAction ?? () {},
                  inactionText: 'Cancel',
                  onInaction: onInaction ?? () {},
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
  }

  testWidgets('tapping action button invokes only onAction', (tester) async {
    var actionCount = 0;
    var inactionCount = 0;
    await openDialog(
      tester,
      onAction: () => actionCount++,
      onInaction: () => inactionCount++,
    );

    await tester.tap(find.text('Delete'));
    await tester.pumpAndSettle();

    expect(actionCount, 1);
    expect(inactionCount, 0);
  });

  testWidgets('tapping inaction button invokes only onInaction',
      (tester) async {
    var actionCount = 0;
    var inactionCount = 0;
    await openDialog(
      tester,
      onAction: () => actionCount++,
      onInaction: () => inactionCount++,
    );

    await tester.tap(find.text('Cancel'));
    await tester.pumpAndSettle();

    expect(actionCount, 0);
    expect(inactionCount, 1);
  });

  testWidgets('tapping close button dismisses without invoking callbacks',
      (tester) async {
    var callbackCount = 0;
    await openDialog(
      tester,
      onAction: () => callbackCount++,
      onInaction: () => callbackCount++,
    );
    expect(find.text('Delete instance'), findsOneWidget);
    expect(find.text('Are you sure?'), findsOneWidget);

    await tester.tap(find.byIcon(Icons.close));
    await tester.pumpAndSettle();

    expect(find.text('Delete instance'), findsNothing);
    expect(callbackCount, 0);
  });
}
