import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_action_buttons.dart';

class _FakeGrpcClient extends GrpcClient {
  final List<({String method, String name})> calls = [];

  _FakeGrpcClient() : super(RpcClient(ClientChannel('localhost')));

  @override
  Future<StartReply?> start(Iterable<String> names) async {
    calls.add((method: 'start', name: names.first));
    return StartReply();
  }

  @override
  Future<StopReply?> stop(Iterable<String> names) async {
    calls.add((method: 'stop', name: names.first));
    return StopReply();
  }

  @override
  Future<SuspendReply?> suspend(Iterable<String> names) async {
    calls.add((method: 'suspend', name: names.first));
    return SuspendReply();
  }

  @override
  Future<DeleteReply?> purge(Iterable<String> names) async {
    calls.add((method: 'purge', name: names.first));
    return DeleteReply();
  }
}

void main() {
  const vmName = 'test-vm';
  late _FakeGrpcClient fakeClient;

  setUp(() => fakeClient = _FakeGrpcClient());

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
      expect(fakeClient.calls, contains((method: 'stop', name: vmName)));
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
      expect(fakeClient.calls, contains((method: 'start', name: vmName)));
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
      expect(fakeClient.calls, contains((method: 'suspend', name: vmName)));
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
  });
}
