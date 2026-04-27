import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_action.dart';

Widget _buildButton({
  required VmAction action,
  required Iterable<Status> statuses,
  VoidCallback? onPressed,
}) {
  return MaterialApp(
    locale: const Locale('en'),
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

OutlinedButton _outlinedButton(WidgetTester tester) =>
    tester.widget<OutlinedButton>(find.byType(OutlinedButton));

void main() {
  group('VmActionButton — start', () {
    testWidgets('enabled when status is STOPPED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.start, statuses: [Status.STOPPED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('enabled when status is SUSPENDED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.start, statuses: [Status.SUSPENDED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('disabled when status is RUNNING', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.start, statuses: [Status.RUNNING]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNull);
    });
  });

  group('VmActionButton — stop', () {
    testWidgets('enabled when status is RUNNING', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.stop, statuses: [Status.RUNNING]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('disabled when status is STOPPED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.stop, statuses: [Status.STOPPED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNull);
    });

    testWidgets('disabled when status is SUSPENDED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.stop, statuses: [Status.SUSPENDED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNull);
    });
  });

  group('VmActionButton — suspend', () {
    testWidgets('enabled when status is RUNNING', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.suspend, statuses: [Status.RUNNING]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('disabled when status is STOPPED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.suspend, statuses: [Status.STOPPED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNull);
    });
  });

  group('VmActionButton — delete', () {
    testWidgets('enabled when status is STOPPED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.delete, statuses: [Status.STOPPED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('enabled when status is RUNNING', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.delete, statuses: [Status.RUNNING]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('enabled when status is SUSPENDED', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.delete, statuses: [Status.SUSPENDED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('disabled when no matching statuses', (tester) async {
      await tester.pumpWidget(
        _buildButton(action: VmAction.delete, statuses: [Status.DELETED]),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNull);
    });
  });

  group('VmActionButton — function callback', () {
    testWidgets('invokes function when enabled and tapped', (tester) async {
      var tapped = false;
      await tester.pumpWidget(
        _buildButton(
          action: VmAction.start,
          statuses: [Status.STOPPED],
          onPressed: () => tapped = true,
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.byType(OutlinedButton));
      await tester.pump();

      expect(tapped, isTrue);
    });

    testWidgets('does not invoke function when disabled', (tester) async {
      var tapped = false;
      await tester.pumpWidget(
        _buildButton(
          action: VmAction.start,
          statuses: [Status.RUNNING],
          onPressed: () => tapped = true,
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.byType(OutlinedButton));
      await tester.pump();

      expect(tapped, isFalse);
    });
  });

  group('VmActionButton — mixed statuses', () {
    testWidgets('start enabled when at least one status is STOPPED',
        (tester) async {
      await tester.pumpWidget(
        _buildButton(
          action: VmAction.start,
          statuses: [Status.RUNNING, Status.STOPPED],
        ),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNotNull);
    });

    testWidgets('stop disabled when no statuses are RUNNING', (tester) async {
      await tester.pumpWidget(
        _buildButton(
          action: VmAction.stop,
          statuses: [Status.STOPPED, Status.SUSPENDED],
        ),
      );
      await tester.pumpAndSettle();

      expect(_outlinedButton(tester).onPressed, isNull);
    });
  });
}
