import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/zone_dropdown.dart';
import 'package:multipass_gui/dropdown.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';

final _l10n = AppLocalizationsEn();

final _zoneA = Zone(name: 'zone-a', available: true);
final _zoneB = Zone(name: 'zone-b', available: true);
final _unavailableZone = Zone(name: 'zone-c', available: false);

Widget _buildDropdown(
  List<Zone> zones, {
  String value = '',
  ValueChanged<String?>? onChanged,
}) {
  return ProviderScope(
    overrides: [zonesProvider.overrideWithValue(BuiltList(zones))],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: ZoneDropdown(value: value, onChanged: onChanged ?? (_) {}),
      ),
    ),
  );
}

Dropdown<String> _dropdown(WidgetTester tester) =>
    tester.widget(find.byType(Dropdown<String>));

void main() {
  group('ZoneDropdown', () {
    testWidgets('lists every zone and marks the unavailable ones', (
      tester,
    ) async {
      await tester.pumpWidget(
        _buildDropdown([_zoneA, _unavailableZone], value: 'zone-a'),
      );

      expect(_dropdown(tester).items, {
        'zone-a': 'zone-a',
        'zone-c': _l10n.zoneDropdownUnavailableZone('zone-c'),
      });
    });

    testWidgets('selects the first available zone when none is selected', (
      tester,
    ) async {
      final selections = <String?>[];
      await tester.pumpWidget(
        _buildDropdown([
          _unavailableZone,
          _zoneB,
          _zoneA,
        ], onChanged: selections.add),
      );

      expect(selections, ['zone-b']);
      expect(find.text('zone-b'), findsOneWidget);
    });

    testWidgets('keeps the selected zone', (tester) async {
      final selections = <String?>[];
      await tester.pumpWidget(
        _buildDropdown(
          [_zoneA, _zoneB],
          value: 'zone-b',
          onChanged: selections.add,
        ),
      );

      expect(selections, isEmpty);
      expect(find.text('zone-b'), findsOneWidget);
    });

    testWidgets('shows no error while selecting the default zone', (
      tester,
    ) async {
      await tester.pumpWidget(_buildDropdown([_zoneA]));

      expect(_dropdown(tester).errorText, isNull);
    });

    testWidgets('shows an error when the selected zone is unavailable', (
      tester,
    ) async {
      await tester.pumpWidget(
        _buildDropdown([_zoneA, _unavailableZone], value: 'zone-c'),
      );

      expect(
        find.text(_l10n.zoneDropdownSelectedZoneUnavailableError('zone-c')),
        findsOneWidget,
      );
    });

    testWidgets('shows an error and selects nothing when all zones are '
        'unavailable', (tester) async {
      final selections = <String?>[];
      await tester.pumpWidget(
        _buildDropdown([_unavailableZone], onChanged: selections.add),
      );

      expect(selections, isEmpty);
      expect(
        find.text(_l10n.zoneDropdownAllZonesUnavailableError),
        findsOneWidget,
      );
    });
  });
}
