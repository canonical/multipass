import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details.dart';
import 'package:multipass_gui/vm_details/vm_details_bridge.dart';

const _vmName = 'test-vm';
const _bridgedKey = (name: _vmName, resource: VmResource.bridged);

final _l10n = AppLocalizationsEn();

class _FakeVmResourceNotifier extends VmResourceNotifier {
  _FakeVmResourceNotifier(super.arg, this._value, this._setCalls);
  final String _value;
  final List<String> _setCalls;

  @override
  Future<String> build() async => _value;

  @override
  Future<void> set(String value) async => _setCalls.add(value);
}

Future<ProviderContainer> _pumpBridge(
  WidgetTester tester, {
  Status status = Status.STOPPED,
  bool bridged = false,
  Set<String> networks = const {'eth0'},
  String bridgedNetwork = 'eth0',
  List<String>? bridgedSets,
}) async {
  final container = ProviderContainer(
    overrides: [
      vmInfoProvider(_vmName).overrideWithBuild(
        (ref, _) => DetailedInfoItem(
          instanceStatus: InstanceStatus(status: status),
        ),
      ),
      vmResourceProvider(_bridgedKey).overrideWith(
        () => _FakeVmResourceNotifier(
          _bridgedKey,
          bridged.toString(),
          bridgedSets ?? [],
        ),
      ),
      networksProvider.overrideWith((_) async => BuiltSet<String>(networks)),
      daemonSettingProvider('local.bridged-network').overrideWithBuild(
        (ref, _) => bridgedNetwork,
      ),
    ],
  );
  addTearDown(container.dispose);
  // Stands in for VmDetails, which keeps this autoDispose provider alive.
  container.listen(activeEditPageProvider(_vmName), (_, __) {});

  await tester.pumpWidget(
    UncontrolledProviderScope(
      container: container,
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: const Scaffold(
          body: SizedBox(width: 600, child: BridgedDetails(_vmName)),
        ),
      ),
    ),
  );
  await tester.pumpAndSettle();
  return container;
}

Future<void> _tapButton(WidgetTester tester, String text) async {
  await tester.tap(find.text(text));
  await tester.pumpAndSettle();
}

OutlinedButton _configureButton(WidgetTester tester) => tester.widget(
      find.widgetWithText(OutlinedButton, _l10n.commonConfigure),
    );

void main() {
  group('BridgedDetails', () {
    for (final (bridged, status) in [
      (false, _l10n.bridgeStatusNotConnected),
      (true, _l10n.bridgeStatusConnected),
    ]) {
      testWidgets('shows the status when bridged=$bridged', (tester) async {
        await _pumpBridge(tester, bridged: bridged);

        expect(find.text(status), findsOneWidget);
      });
    }

    testWidgets('hides Configure once bridged', (tester) async {
      await _pumpBridge(tester, bridged: true);

      expect(find.text(_l10n.commonConfigure), findsNothing);
    });

    for (final status in [Status.RUNNING, Status.SUSPENDED, Status.STARTING]) {
      testWidgets('disables Configure when status is ${status.name}',
          (tester) async {
        await _pumpBridge(tester, status: status);

        expect(_configureButton(tester).onPressed, isNull);
      });
    }

    testWidgets('Configure enters editing and claims the edit page',
        (tester) async {
      final container = await _pumpBridge(tester);

      await _tapButton(tester, _l10n.commonConfigure);

      expect(find.byType(CheckboxListTile), findsOneWidget);
      expect(find.text(_l10n.commonCancel), findsOneWidget);
      expect(
        container.read(activeEditPageProvider(_vmName)),
        ActiveEditPage.bridge,
      );
    });

    for (final (description, networks, bridgedNetwork, message, enabled) in [
      ('no networks exist', <String>{}, '', _l10n.bridgeNoNetworks, false),
      (
        'the saved network is unavailable',
        {'eth0'},
        'eth1',
        _l10n.bridgeNoValidNetwork,
        false,
      ),
      (
        'the saved network is available',
        {'eth0'},
        'eth0',
        _l10n.bridgeEstablishedWarning,
        true,
      ),
    ]) {
      testWidgets('checkbox enabled=$enabled when $description',
          (tester) async {
        await _pumpBridge(
          tester,
          networks: networks,
          bridgedNetwork: bridgedNetwork,
        );

        await _tapButton(tester, _l10n.commonConfigure);

        final checkbox =
            tester.widget<CheckboxListTile>(find.byType(CheckboxListTile));
        expect(checkbox.enabled, enabled);
        expect(find.text(message), findsOneWidget);
      });
    }

    testWidgets('saving a checked box bridges and releases the edit page',
        (tester) async {
      final bridgedSets = <String>[];
      final container = await _pumpBridge(tester, bridgedSets: bridgedSets);

      await _tapButton(tester, _l10n.commonConfigure);
      await tester.tap(find.byType(CheckboxListTile));
      await _tapButton(tester, _l10n.commonSave);

      expect(bridgedSets, ['true']);
      expect(find.byType(CheckboxListTile), findsNothing);
      expect(container.read(activeEditPageProvider(_vmName)), isNull);
    });

    testWidgets('saving an unchecked box does not bridge', (tester) async {
      final bridgedSets = <String>[];
      await _pumpBridge(tester, bridgedSets: bridgedSets);

      await _tapButton(tester, _l10n.commonConfigure);
      await _tapButton(tester, _l10n.commonSave);

      expect(bridgedSets, isEmpty);
    });

    testWidgets('Cancel discards the change and releases the edit page',
        (tester) async {
      final bridgedSets = <String>[];
      final container = await _pumpBridge(tester, bridgedSets: bridgedSets);

      await _tapButton(tester, _l10n.commonConfigure);
      await tester.tap(find.byType(CheckboxListTile));
      await _tapButton(tester, _l10n.commonCancel);

      expect(bridgedSets, isEmpty);
      expect(find.byType(CheckboxListTile), findsNothing);
      expect(container.read(activeEditPageProvider(_vmName)), isNull);
    });

    testWidgets('stops editing when the instance leaves STOPPED',
        (tester) async {
      final container = await _pumpBridge(tester);
      await _tapButton(tester, _l10n.commonConfigure);

      container.read(vmInfoProvider(_vmName).notifier).state = DetailedInfoItem(
        instanceStatus: InstanceStatus(status: Status.STARTING),
      );
      await tester.pumpAndSettle();

      expect(find.byType(CheckboxListTile), findsNothing);
      expect(_configureButton(tester).onPressed, isNull);
    });
  });
}
