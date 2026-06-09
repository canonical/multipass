import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_action.dart';

void main() {
  group('VmAction.allowedStatuses', () {
    const cases = [
      (VmAction.start, {Status.STOPPED, Status.SUSPENDED}),
      (VmAction.stop, {Status.RUNNING}),
      (VmAction.suspend, {Status.RUNNING}),
      (VmAction.restart, {Status.RUNNING}),
      (VmAction.delete, {Status.STOPPED, Status.SUSPENDED, Status.RUNNING}),
      (VmAction.recover, {Status.DELETED}),
      (VmAction.purge, {Status.DELETED}),
      (VmAction.edit, {Status.STOPPED}),
    ];

    for (final (action, expectedStatuses) in cases) {
      test('${action.name} allows the expected statuses', () {
        expect(action.allowedStatuses, equals(expectedStatuses));
      });
    }
  });

  group('VmActionButton', () {
    Widget buildButton({
      required VmAction action,
      required List<Status> statuses,
      VoidCallback? onPressed,
    }) {
      return MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: VmActionButton(
            action: action,
            currentStatuses: statuses,
            function: onPressed ?? () {},
          ),
        ),
      );
    }

    testWidgets('is enabled when current status is in allowedStatuses',
        (WidgetTester tester) async {
      await tester.pumpWidget(buildButton(
        action: VmAction.start,
        statuses: [Status.STOPPED],
      ));

      final button = tester.widget<OutlinedButton>(find.byType(OutlinedButton));
      expect(button.onPressed, isNotNull);
    });

    testWidgets('is disabled when no current status is in allowedStatuses',
        (WidgetTester tester) async {
      await tester.pumpWidget(buildButton(
        action: VmAction.start,
        statuses: [Status.RUNNING],
      ));

      final button = tester.widget<OutlinedButton>(find.byType(OutlinedButton));
      expect(button.onPressed, isNull);
    });

    testWidgets('is enabled when at least one status matches allowedStatuses',
        (WidgetTester tester) async {
      // delete allows STOPPED, SUSPENDED, or RUNNING
      await tester.pumpWidget(buildButton(
        action: VmAction.delete,
        statuses: [Status.RUNNING],
      ));

      final button = tester.widget<OutlinedButton>(find.byType(OutlinedButton));
      expect(button.onPressed, isNotNull);
    });

    testWidgets('invokes callback when tapped while enabled',
        (WidgetTester tester) async {
      var tapped = false;

      await tester.pumpWidget(buildButton(
        action: VmAction.stop,
        statuses: [Status.RUNNING],
        onPressed: () => tapped = true,
      ));

      await tester.tap(find.byType(OutlinedButton));
      expect(tapped, isTrue);
    });

    testWidgets('does not invoke callback when tapped while disabled',
        (WidgetTester tester) async {
      var tapped = false;

      await tester.pumpWidget(buildButton(
        action: VmAction.stop,
        statuses: [Status.STOPPED],
        onPressed: () => tapped = true,
      ));

      await tester.tap(find.byType(OutlinedButton));
      expect(tapped, isFalse);
    });

    testWidgets('is disabled when statuses list is empty',
        (WidgetTester tester) async {
      await tester.pumpWidget(buildButton(
        action: VmAction.start,
        statuses: [],
      ));

      final button = tester.widget<OutlinedButton>(find.byType(OutlinedButton));
      expect(button.onPressed, isNull);
    });
  });
}
