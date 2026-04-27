import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_table/bulk_actions.dart';
import 'package:multipass_gui/vm_table/vms.dart';

class _FakeGrpcClient extends GrpcClient {
  final List<({String method, Iterable<String> names})> calls = [];

  _FakeGrpcClient() : super(RpcClient(ClientChannel('localhost')));

  @override
  Future<StartReply?> start(Iterable<String> names) async {
    calls.add((method: 'start', names: names));
    return StartReply();
  }

  @override
  Future<StopReply?> stop(Iterable<String> names) async {
    calls.add((method: 'stop', names: names));
    return StopReply();
  }

  @override
  Future<SuspendReply?> suspend(Iterable<String> names) async {
    calls.add((method: 'suspend', names: names));
    return SuspendReply();
  }

  @override
  Future<DeleteReply?> purge(Iterable<String> names) async {
    calls.add((method: 'purge', names: names));
    return DeleteReply();
  }
}

Widget _buildApp({
  required _FakeGrpcClient client,
  BuiltSet<String>? selected,
  Map<String, Status> statuses = const {},
}) {
  return ProviderScope(
    overrides: [
      grpcClientProvider.overrideWithValue(client),
      selectedVmsProvider.overrideWithBuild(
        (ref, _) => selected ?? BuiltSet(),
      ),
      vmStatusesProvider.overrideWith(
        (ref) => BuiltMap<String, Status>(statuses),
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
            return const BulkActionsBar();
          },
        ),
      ),
    ),
  );
}

void main() {
  late _FakeGrpcClient fakeClient;
  setUp(() => fakeClient = _FakeGrpcClient());

  group('BulkActionsBar', () {
    testWidgets('renders four action buttons', (tester) async {
      await tester.pumpWidget(_buildApp(client: fakeClient));
      await tester.pump();

      expect(find.byType(OutlinedButton), findsNWidgets(4));
    });

    testWidgets('all buttons disabled when no VMs selected', (tester) async {
      await tester.pumpWidget(
        _buildApp(client: fakeClient, selected: BuiltSet()),
      );
      await tester.pump();

      final buttons = tester
          .widgetList<OutlinedButton>(find.byType(OutlinedButton))
          .toList();
      for (final button in buttons) {
        expect(button.onPressed, isNull);
      }
    });

    testWidgets('Start enabled when selected VM is STOPPED', (tester) async {
      await tester.pumpWidget(_buildApp(
        client: fakeClient,
        selected: BuiltSet({'vm1'}),
        statuses: {'vm1': Status.STOPPED},
      ));
      await tester.pump();

      final start = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Start'),
      );
      expect(start.onPressed, isNotNull);
    });

    testWidgets('Start disabled when selected VM is RUNNING', (tester) async {
      await tester.pumpWidget(_buildApp(
        client: fakeClient,
        selected: BuiltSet({'vm1'}),
        statuses: {'vm1': Status.RUNNING},
      ));
      await tester.pump();

      final start = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Start'),
      );
      expect(start.onPressed, isNull);
    });

    testWidgets('Stop enabled when selected VM is RUNNING', (tester) async {
      await tester.pumpWidget(_buildApp(
        client: fakeClient,
        selected: BuiltSet({'vm1'}),
        statuses: {'vm1': Status.RUNNING},
      ));
      await tester.pump();

      final stop = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Stop'),
      );
      expect(stop.onPressed, isNotNull);
    });

    testWidgets('Delete enabled when selected VM is STOPPED', (tester) async {
      await tester.pumpWidget(_buildApp(
        client: fakeClient,
        selected: BuiltSet({'vm1'}),
        statuses: {'vm1': Status.STOPPED},
      ));
      await tester.pump();

      final delete = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Delete'),
      );
      expect(delete.onPressed, isNotNull);
    });

    testWidgets('tapping Start calls client.start() with selected VM names',
        (tester) async {
      await tester.pumpWidget(_buildApp(
        client: fakeClient,
        selected: BuiltSet({'vm1'}),
        statuses: {'vm1': Status.STOPPED},
      ));
      await tester.pump();

      await tester.tap(find.widgetWithText(OutlinedButton, 'Start'));
      await tester.pump();

      expect(fakeClient.calls, hasLength(1));
      expect(fakeClient.calls.first.method, equals('start'));
      expect(fakeClient.calls.first.names, containsAll(['vm1']));
    });

    testWidgets('tapping Stop calls client.stop() with selected VM names',
        (tester) async {
      await tester.pumpWidget(_buildApp(
        client: fakeClient,
        selected: BuiltSet({'vm1'}),
        statuses: {'vm1': Status.RUNNING},
      ));
      await tester.pump();

      await tester.tap(find.widgetWithText(OutlinedButton, 'Stop'));
      await tester.pump();

      expect(fakeClient.calls, hasLength(1));
      expect(fakeClient.calls.first.method, equals('stop'));
      expect(fakeClient.calls.first.names, containsAll(['vm1']));
    });
  });
}
