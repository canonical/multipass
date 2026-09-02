import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:riverpod/misc.dart' show Override;
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_table/bulk_actions.dart';
import 'package:multipass_gui/vm_table/zones_dropdown_button.dart';

class RecordingGrpcClient extends GrpcClient {
  RecordingGrpcClient()
      : super(RpcClient(ClientChannel('localhost', port: 1)));

  final calls = <({List<String> zones, bool available})>[];

  @override
  Future<ZonesStateReply?> zonesState(List<String> zones, bool available) async {
    calls.add((zones: List<String>.from(zones), available: available));
    return null;
  }
}

Widget buildTestApp({
  required Widget child,
  required List<Override> overrides,
}) {
  return ProviderScope(
    overrides: overrides,
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(body: child),
    ),
  );
}

Zone zone({
  required String name,
  required bool available,
  bool supported = true,
}) {
  return Zone(name: name, available: available, supported: supported);
}

DetailedInfoItem vm({
  required String name,
  required String zoneName,
  required Status status,
}) {
  return DetailedInfoItem(
    name: name,
    zone: Zone(name: zoneName),
    instanceStatus: InstanceStatus(status: status),
  );
}

void main() {
  group('ZonesDropdownButton', () {
    testWidgets('renders nothing when no zones are available',
        (WidgetTester tester) async {
      await tester.pumpWidget(
        buildTestApp(
          child: const ZonesDropdownButton(),
          overrides: [
            zonesProvider.overrideWithValue(const <Zone>[].toBuiltList()),
            vmInfosProvider.overrideWithValue(const <DetailedInfoItem>[]),
            grpcClientProvider.overrideWithValue(RecordingGrpcClient()),
          ],
        ),
      );
      await tester.pumpAndSettle();

      expect(
        find.byWidgetPredicate(
          (widget) => widget is SizedBox && widget.width == 0 && widget.height == 0,
        ),
        findsOneWidget,
      );
      expect(
        find.byWidgetPredicate((widget) => widget is PopupMenuButton),
        findsNothing,
      );
      expect(find.byType(OutlinedButton), findsNothing);
    });

    testWidgets('renders the zones button when zones are available',
        (WidgetTester tester) async {
      final client = RecordingGrpcClient();
      final zones = [
        zone(name: 'zone-a', available: true),
      ];

      await tester.pumpWidget(
        buildTestApp(
          child: const ZonesDropdownButton(),
          overrides: [
            zonesProvider.overrideWithValue(zones.toBuiltList()),
            vmInfosProvider.overrideWithValue(const <DetailedInfoItem>[]),
            grpcClientProvider.overrideWithValue(client),
          ],
        ),
      );
      await tester.pumpAndSettle();

      final context = tester.element(find.byType(Scaffold));
      final l10n = AppLocalizations.of(context)!;

      expect(
        find.byWidgetPredicate((widget) => widget is PopupMenuButton),
        findsOneWidget,
      );
      expect(find.text(l10n.vmTableZonesButtonTitle), findsOneWidget);
      expect(find.byIcon(Icons.warning_rounded), findsNothing);
      expect(find.text(l10n.vmTableZonesUnavailableLabel(0, zones.length)),
          findsNothing);
    });

    testWidgets('shows the unavailable warning when some zones are disabled',
        (WidgetTester tester) async {
      final zones = [
        zone(name: 'zone-a', available: true),
        zone(name: 'zone-b', available: false),
        zone(name: 'zone-c', available: true),
      ];

      await tester.pumpWidget(
        buildTestApp(
          child: const ZonesDropdownButton(),
          overrides: [
            zonesProvider.overrideWithValue(zones.toBuiltList()),
            vmInfosProvider.overrideWithValue(const <DetailedInfoItem>[]),
            grpcClientProvider.overrideWithValue(RecordingGrpcClient()),
          ],
        ),
      );
      await tester.pumpAndSettle();

      final context = tester.element(find.byType(Scaffold));
      final l10n = AppLocalizations.of(context)!;

      expect(find.byIcon(Icons.warning_rounded), findsOneWidget);
      expect(
        find.text(l10n.vmTableZonesUnavailableLabel(1, zones.length)),
        findsOneWidget,
      );
    });

    testWidgets('hides the unavailable warning when all zones are enabled',
        (WidgetTester tester) async {
      final zones = [
        zone(name: 'zone-a', available: true),
        zone(name: 'zone-b', available: true),
      ];

      await tester.pumpWidget(
        buildTestApp(
          child: const ZonesDropdownButton(),
          overrides: [
            zonesProvider.overrideWithValue(zones.toBuiltList()),
            vmInfosProvider.overrideWithValue(const <DetailedInfoItem>[]),
            grpcClientProvider.overrideWithValue(RecordingGrpcClient()),
          ],
        ),
      );
      await tester.pumpAndSettle();

      expect(find.byIcon(Icons.warning_rounded), findsNothing);
    });

    testWidgets('popup shows zone rows, counts running VMs, and toggles state',
        (WidgetTester tester) async {
      final client = RecordingGrpcClient();
      final zones = [
        zone(name: 'zone-a', available: true),
        zone(name: 'zone-b', available: false),
        zone(name: 'zone-c', available: true),
      ];
      final infos = [
        vm(name: 'vm-a-running', zoneName: 'zone-a', status: Status.RUNNING),
        vm(name: 'vm-a-stopped', zoneName: 'zone-a', status: Status.STOPPED),
        vm(name: 'vm-b-running-1', zoneName: 'zone-b', status: Status.RUNNING),
        vm(name: 'vm-b-running-2', zoneName: 'zone-b', status: Status.RUNNING),
        vm(name: 'vm-b-suspended', zoneName: 'zone-b', status: Status.SUSPENDED),
        vm(name: 'vm-c-stopped', zoneName: 'zone-c', status: Status.STOPPED),
      ];

      await tester.pumpWidget(
        buildTestApp(
          child: const ZonesDropdownButton(),
          overrides: [
            zonesProvider.overrideWithValue(zones.toBuiltList()),
            vmInfosProvider.overrideWithValue(infos),
            grpcClientProvider.overrideWithValue(client),
          ],
        ),
      );
      await tester.pumpAndSettle();

      final context = tester.element(find.byType(Scaffold));
      final l10n = AppLocalizations.of(context)!;

      await tester.tap(find.byTooltip(l10n.vmTableZonesButtonTooltip));
      await tester.pumpAndSettle();

      expect(find.text(l10n.vmTableZonesPopupTitle), findsOneWidget);
      expect(find.text('zone-a'), findsOneWidget);
      expect(find.text('zone-b'), findsOneWidget);
      expect(find.text('zone-c'), findsOneWidget);
      expect(find.text(l10n.vmTableZonesRunningInstanceCount(1)), findsOneWidget);
      expect(find.text(l10n.vmTableZonesRunningInstanceCount(2)), findsOneWidget);
      expect(find.text(l10n.vmTableZonesRunningInstanceCount(0)), findsOneWidget);

      final switches = tester.widgetList<CupertinoSwitch>(
        find.byType(CupertinoSwitch),
      ).toList();
      expect(switches, hasLength(3));
      expect(switches.map((s) => s.value).toList(), [true, false, true]);
      expect(switches.first.onChanged, isNotNull);

      switches.first.onChanged!(false);

      expect(client.calls, hasLength(1));
      expect(client.calls.single.zones, ['zone-a']);
      expect(client.calls.single.available, isFalse);
    });
  });

  group('BulkActionsBar', () {
    testWidgets('includes the zones dropdown button',
        (WidgetTester tester) async {
      await tester.pumpWidget(
        buildTestApp(
          child: const BulkActionsBar(),
          overrides: [
            zonesProvider.overrideWithValue(
              [zone(name: 'zone-a', available: true)].toBuiltList(),
            ),
            vmInfosProvider.overrideWithValue(const <DetailedInfoItem>[]),
            grpcClientProvider.overrideWithValue(RecordingGrpcClient()),
          ],
        ),
      );
      await tester.pumpAndSettle();

      final context = tester.element(find.byType(Scaffold));
      final l10n = AppLocalizations.of(context)!;

      expect(find.byType(ZonesDropdownButton), findsOneWidget);
      expect(find.text(l10n.vmTableZonesButtonTitle), findsOneWidget);
    });
  });
}
