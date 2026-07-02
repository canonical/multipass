import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_action.dart';
import 'package:multipass_gui/vm_details/vm_action_buttons.dart';

void main() {
  const vmName = 'test-vm';

  Widget buildWidget({
    required VmAction action,
    required Status status,
    VoidCallback? onTap,
  }) {
    return ProviderScope(
      overrides: [
        vmInfoProvider(vmName).overrideWithBuild(
          (ref, notifier) => DetailedInfoItem(
            instanceStatus: InstanceStatus(status: status),
          ),
        ),
      ],
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: ActionTile(vmName, action, onTap ?? () {}),
        ),
      ),
    );
  }

  group('ActionTile', () {
    void runEnabledTests(VmAction action, List<(Status, bool)> cases) {
      for (final (status, enabled) in cases) {
        testWidgets(
            'is ${enabled ? 'enabled' : 'disabled'} when status is ${status.name}',
            (tester) async {
          await tester.pumpWidget(buildWidget(action: action, status: status));
          await tester.pumpAndSettle();

          final tile = tester.widget<ListTile>(find.byType(ListTile));
          expect(tile.enabled, enabled);
        });
      }
    }

    const allStatuses = [
      Status.UNKNOWN,
      Status.RUNNING,
      Status.STARTING,
      Status.RESTARTING,
      Status.STOPPED,
      Status.DELETED,
      Status.DELAYED_SHUTDOWN,
      Status.SUSPENDING,
      Status.SUSPENDED,
    ];

    group('VmAction.start', () {
      runEnabledTests(VmAction.start, [
        for (final s in allStatuses)
          (s, s == Status.STOPPED || s == Status.SUSPENDED),
      ]);
    });

    group('VmAction.stop', () {
      runEnabledTests(VmAction.stop, [
        for (final s in allStatuses) (s, s == Status.RUNNING),
      ]);
    });

    group('VmAction.suspend', () {
      runEnabledTests(VmAction.suspend, [
        for (final s in allStatuses) (s, s == Status.RUNNING),
      ]);
    });

    group('VmAction.delete', () {
      runEnabledTests(VmAction.delete, [
        for (final s in allStatuses)
          (
            s,
            s == Status.STOPPED || s == Status.SUSPENDED || s == Status.RUNNING
          ),
      ]);
    });

    group('tap behaviour', () {
      testWidgets('invokes callback when tapped while enabled',
          (WidgetTester tester) async {
        var tapped = false;

        await tester.pumpWidget(buildWidget(
          action: VmAction.stop,
          status: Status.RUNNING,
          onTap: () => tapped = true,
        ));
        await tester.pumpAndSettle();

        await tester.tap(find.byType(ListTile));
        await tester.pumpAndSettle();

        expect(tapped, isTrue);
      });

      testWidgets('displays action label as tile title',
          (WidgetTester tester) async {
        await tester.pumpWidget(buildWidget(
          action: VmAction.start,
          status: Status.STOPPED,
        ));
        await tester.pumpAndSettle();

        expect(
          find.descendant(
            of: find.byType(ListTile),
            matching: find.byType(Text),
          ),
          findsOneWidget,
        );
      });

      testWidgets('title text style is not null when enabled',
          (WidgetTester tester) async {
        await tester.pumpWidget(buildWidget(
          action: VmAction.start,
          status: Status.STOPPED,
        ));
        await tester.pumpAndSettle();

        final text = tester.widget<Text>(
          find.descendant(
            of: find.byType(ListTile),
            matching: find.byType(Text),
          ),
        );
        expect(text.style?.color, isNotNull);
      });

      testWidgets('title text style is null when disabled',
          (WidgetTester tester) async {
        await tester.pumpWidget(buildWidget(
          action: VmAction.start,
          status: Status.RUNNING,
        ));
        await tester.pumpAndSettle();

        final text = tester.widget<Text>(
          find.descendant(
            of: find.byType(ListTile),
            matching: find.byType(Text),
          ),
        );
        expect(text.style, isNull);
      });
    });
  });
}
