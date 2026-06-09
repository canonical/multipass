import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_action_buttons.dart';

import '../helpers.dart' show FakeGrpcClient;

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
        locale: const Locale('en'),
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
      expect(find.text('Actions'), findsOneWidget);
    });

    testWidgets('opening the menu shows Start, Stop, Suspend and Delete items',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp());
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      expect(find.text('Start'), findsOneWidget);
      expect(find.text('Stop'), findsOneWidget);
      expect(find.text('Suspend'), findsOneWidget);
      expect(find.text('Delete'), findsOneWidget);
    });

    testWidgets('tapping Stop calls grpcClient.stop with the vm name',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.RUNNING));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      await tester.tap(find.ancestor(
          of: find.text('Stop'), matching: find.byType(ListTile)));
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
          of: find.text('Start'), matching: find.byType(ListTile)));
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
          of: find.text('Suspend'), matching: find.byType(ListTile)));
      await tester.pumpAndSettle();
      expect(fakeClient.calls, hasLength(1));
      expect(fakeClient.calls.first.method, equals('suspend'));
      expect(fakeClient.calls.first.names, contains(vmName));
    });

    testWidgets('Stop is enabled for a RUNNING vm', (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.RUNNING));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      final tile = tester.widget<ListTile>(find.ancestor(
          of: find.text('Stop'), matching: find.byType(ListTile)));
      expect(tile.enabled, isTrue);
    });

    testWidgets('Stop is disabled for a STOPPED vm', (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.STOPPED));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();
      final tile = tester.widget<ListTile>(find.ancestor(
          of: find.text('Stop'), matching: find.byType(ListTile)));
      expect(tile.enabled, isFalse);
    });

    ListTile findTile(WidgetTester tester, String label) =>
        tester.widget<ListTile>(find.ancestor(
            of: find.text(label), matching: find.byType(ListTile)));

    testWidgets('action enabled states are correct for a RUNNING vm',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.RUNNING));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();

      expect(findTile(tester, 'Start').enabled, isFalse);
      expect(findTile(tester, 'Stop').enabled, isTrue);
      expect(findTile(tester, 'Suspend').enabled, isTrue);
      expect(findTile(tester, 'Delete').enabled, isTrue);
    });

    testWidgets('action enabled states are correct for a STOPPED vm',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.STOPPED));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();

      expect(findTile(tester, 'Start').enabled, isTrue);
      expect(findTile(tester, 'Stop').enabled, isFalse);
      expect(findTile(tester, 'Suspend').enabled, isFalse);
      expect(findTile(tester, 'Delete').enabled, isTrue);
    });

    testWidgets('action enabled states are correct for a SUSPENDED vm',
        (tester) async {
      suppressOverflowErrors();
      await tester.pumpWidget(buildApp(vmStatus: Status.SUSPENDED));
      await tester.pump();
      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();

      expect(findTile(tester, 'Start').enabled, isTrue);
      expect(findTile(tester, 'Stop').enabled, isFalse);
      expect(findTile(tester, 'Suspend').enabled, isFalse);
      expect(findTile(tester, 'Delete').enabled, isTrue);
    });
  });
}
