import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/platform/platform.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/settings/virtualization_settings.dart';

final _l10n = AppLocalizationsEn();

class _FakeSettingNotifier extends DaemonSettingNotifier {
  _FakeSettingNotifier(super.arg, this._value, this._setCalls);
  final String _value;
  final List<String> _setCalls;

  @override
  Future<String> build() async => _value;

  @override
  Future<void> set(String value) async => _setCalls.add(value);
}

Widget _buildApp({
  String driver = 'qemu',
  String bridgedNetwork = '',
  Set<String> networks = const {},
  List<String>? driverSets,
  List<String>? bridgedNetworkSets,
}) {
  return ProviderScope(
    overrides: [
      driverProvider.overrideWith(
        () => _FakeSettingNotifier(driverKey, driver, driverSets ?? []),
      ),
      bridgedNetworkProvider.overrideWith(
        () => _FakeSettingNotifier(
          bridgedNetworkKey,
          bridgedNetwork,
          bridgedNetworkSets ?? [],
        ),
      ),
      networksProvider.overrideWith((_) async => BuiltSet<String>(networks)),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const Scaffold(
        body: SizedBox(width: 600, child: VirtualizationSettings()),
      ),
    ),
  );
}

Future<void> _select(WidgetTester tester, String current, String next) async {
  await tester.tap(find.text(current));
  await tester.pumpAndSettle();
  await tester.tap(find.text(next).last);
  await tester.pumpAndSettle();
}

DropdownButton<String> _bridgeDropdown(WidgetTester tester) =>
    tester.widget(find.byType(DropdownButton<String>).last);

void main() {
  group('VirtualizationSettings: driver', () {
    final MapEntry(key: platformDriver, value: platformDriverLabel) =
        mpPlatform.drivers.entries.first;

    testWidgets(
        'shows the current driver even if the platform does not list it',
        (tester) async {
      await tester.pumpWidget(_buildApp(driver: 'unlisted'));
      await tester.pumpAndSettle();

      expect(find.text('unlisted'), findsOneWidget);
    });

    testWidgets('selecting another driver saves it', (tester) async {
      final driverSets = <String>[];
      await tester.pumpWidget(
        _buildApp(driver: 'unlisted', driverSets: driverSets),
      );
      await tester.pumpAndSettle();

      await _select(tester, 'unlisted', platformDriverLabel);

      expect(driverSets, [platformDriver]);
    });

    testWidgets('selecting the current driver does not save it',
        (tester) async {
      final driverSets = <String>[];
      await tester.pumpWidget(
        _buildApp(driver: platformDriver, driverSets: driverSets),
      );
      await tester.pumpAndSettle();

      await _select(tester, platformDriverLabel, platformDriverLabel);

      expect(driverSets, isEmpty);
    });
  });

  group('VirtualizationSettings: bridged network', () {
    testWidgets('is hidden when no networks are available', (tester) async {
      await tester.pumpWidget(_buildApp(networks: {}));
      await tester.pumpAndSettle();

      expect(find.text(_l10n.bridgeTitle), findsNothing);
      expect(find.byType(DropdownButton<String>), findsOneWidget);
    });

    testWidgets('selects the saved network when it is available',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(bridgedNetwork: 'eth0', networks: {'eth0', 'en0'}),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.bridgeTitle), findsOneWidget);
      expect(_bridgeDropdown(tester).value, 'eth0');
    });

    testWidgets('falls back to "None" when the saved network is unavailable',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(bridgedNetwork: 'eth99', networks: {'eth0', 'en0'}),
      );
      await tester.pumpAndSettle();

      expect(_bridgeDropdown(tester).value, isEmpty);
    });

    testWidgets('selecting a network saves it', (tester) async {
      final bridgedNetworkSets = <String>[];
      await tester.pumpWidget(
        _buildApp(
          networks: {'eth0'},
          bridgedNetworkSets: bridgedNetworkSets,
        ),
      );
      await tester.pumpAndSettle();

      await _select(tester, _l10n.virtualizationBridgedNetworkNone, 'eth0');

      expect(bridgedNetworkSets, ['eth0']);
    });
  });
}
