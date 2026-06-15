import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_action_buttons.dart';

import '../helpers.dart' show FakeGrpcClient;

final _l10n = AppLocalizationsEn();

void main() {
  const vmName = 'test-vm';
  late FakeGrpcClient fakeClient;

  setUp(() => fakeClient = FakeGrpcClient());

  Widget buildApp({Status vmStatus = Status.RUNNING}) {
    return ProviderScope(
      overrides: [
        grpcClientProvider.overrideWithValue(fakeClient),
        vmInfoProvider(vmName).overrideWithBuild(
          (ref, notifier) => DetailedInfoItem(
            instanceStatus: InstanceStatus(status: vmStatus),
          ),
        ),
      ],
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: Consumer(
            builder: (context, ref, _) {
              ref.watch(notificationsProvider);
              return VmActionButtons(vmName);
            },
          ),
        ),
      ),
    );
  }

  group('VmActionButtons', () {
    // VmActionButtons has a fixed-width (110px) container and the testing
    // environment uses a different font then production code causing a
    // layout overflow.
    void suppressOverflowErrors() {
      final original = FlutterError.onError;
      FlutterError.onError = (details) {
        if (details.exceptionAsString().contains('overflowed')) return;
        original?.call(details);
      };
      addTearDown(() => FlutterError.onError = original);
    }

    testWidgets('renders the "Actions" label', (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp());
      await tester.pump();
      expect(find.text(_l10n.vmActionsMenuTitle), findsOneWidget);
    });

    testWidgets('opening the menu shows Start, Stop, Suspend and Delete items',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp());
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      expect(find.text(_l10n.vmActionLabel('start')), findsOneWidget);
      expect(find.text(_l10n.vmActionLabel('stop')), findsOneWidget);
      expect(find.text(_l10n.vmActionLabel('suspend')), findsOneWidget);
      expect(find.text(_l10n.vmActionLabel('delete')), findsOneWidget);
    });

    testWidgets('tapping Stop calls grpcClient.stop with the vm name',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.RUNNING));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      await tester.tap(find.ancestor(
          of: find.text(_l10n.vmActionLabel('stop')),
          matching: find.byType(ListTile)));
      await tester.pumpAndSettle();
      expect(fakeClient.calls, hasLength(1));
      expect(fakeClient.calls.first.method, equals('stop'));
      expect(fakeClient.calls.first.names, contains(vmName));
    });

    testWidgets('tapping Start calls grpcClient.start with the vm name',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.STOPPED));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      await tester.tap(find.ancestor(
          of: find.text(_l10n.vmActionLabel('start')),
          matching: find.byType(ListTile)));
      await tester.pumpAndSettle();
      expect(fakeClient.calls, hasLength(1));
      expect(fakeClient.calls.first.method, equals('start'));
      expect(fakeClient.calls.first.names, contains(vmName));
    });

    testWidgets('tapping Suspend calls grpcClient.suspend with the vm name',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.RUNNING));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      await tester.tap(find.ancestor(
          of: find.text(_l10n.vmActionLabel('suspend')),
          matching: find.byType(ListTile)));
      await tester.pumpAndSettle();
      expect(fakeClient.calls, hasLength(1));
      expect(fakeClient.calls.first.method, equals('suspend'));
      expect(fakeClient.calls.first.names, contains(vmName));
    });

    ListTile findTile(WidgetTester tester, String label) =>
        tester.widget<ListTile>(find.ancestor(
            of: find.text(label), matching: find.byType(ListTile)));

    const actionEnabledCases = [
      (
        status: Status.RUNNING,
        start: false,
        stop: true,
        suspend: true,
        delete: true,
      ),
      (
        status: Status.STOPPED,
        start: true,
        stop: false,
        suspend: false,
        delete: true,
      ),
      (
        status: Status.SUSPENDED,
        start: true,
        stop: false,
        suspend: false,
        delete: true,
      ),
    ];

    for (final tc in actionEnabledCases) {
      testWidgets(
          'action enabled states are correct for a ${tc.status.name} vm',
          (tester) async {
        suppressOverflowErrors();
        await tester.pumpWidget(buildApp(vmStatus: tc.status));
        await tester.pump();
        await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
        await tester.pumpAndSettle();

        expect(
            findTile(tester, _l10n.vmActionLabel('start')).enabled, tc.start);
        expect(findTile(tester, _l10n.vmActionLabel('stop')).enabled, tc.stop);
        expect(findTile(tester, _l10n.vmActionLabel('suspend')).enabled,
            tc.suspend);
        expect(
            findTile(tester, _l10n.vmActionLabel('delete')).enabled, tc.delete);
      });
    }
  });
}
