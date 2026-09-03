import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:riverpod/misc.dart' show Override;
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/zone_dropdown.dart';
import 'package:multipass_gui/dropdown.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';

void main() {
  Widget wrapWithApp(
    Widget child, {
    List<Override> overrides = const [],
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

  Widget buildZoneDropdown({
    required List<Zone> zones,
    required String value,
    required ValueChanged<String?>? onChanged,
    bool enabled = true,
  }) {
    return wrapWithApp(
      ZoneDropdown(
        value: value,
        onChanged: onChanged,
        enabled: enabled,
      ),
      overrides: [
        zonesProvider.overrideWithValue(zones.toBuiltList()),
      ],
    );
  }

  group('ZoneDropdown', () {
    testWidgets('selects the first available zone by default',
        (WidgetTester tester) async {
      String? selectedZone;
      final zones = [
        Zone(name: 'zone-a', available: false, supported: true),
        Zone(name: 'zone-b', available: true, supported: true),
        Zone(name: 'zone-c', available: true, supported: true),
      ];

      await tester.pumpWidget(
        buildZoneDropdown(
          zones: zones,
          value: '',
          onChanged: (value) => selectedZone = value,
        ),
      );
      await tester.pumpAndSettle();

      final l10n =
          AppLocalizations.of(tester.element(find.byType(ZoneDropdown)))!;
      expect(selectedZone, 'zone-b');
      expect(find.text(l10n.zoneDropdownTitle), findsOneWidget);
      expect(
        tester
            .widget<DropdownButton<String>>(find.byType(DropdownButton<String>))
            .value,
        'zone-b',
      );
      expect(find.text(l10n.zoneDropdownSelectedZoneUnavailableError('zone-b')),
          findsNothing);
    });

    testWidgets('shows the selected zone as unavailable when needed',
        (WidgetTester tester) async {
      final zones = [
        Zone(name: 'zone-a', available: false, supported: true),
        Zone(name: 'zone-b', available: true, supported: true),
      ];

      await tester.pumpWidget(
        buildZoneDropdown(
          zones: zones,
          value: 'zone-a',
          onChanged: (_) {},
        ),
      );
      await tester.pumpAndSettle();

      final l10n =
          AppLocalizations.of(tester.element(find.byType(ZoneDropdown)))!;
      expect(
        find.text(l10n.zoneDropdownUnavailableZone('zone-a')),
        findsWidgets,
      );
      expect(
        find.text(l10n.zoneDropdownSelectedZoneUnavailableError('zone-a')),
        findsOneWidget,
      );
      expect(
        tester
            .widget<DropdownButton<String>>(find.byType(DropdownButton<String>))
            .value,
        'zone-a',
      );
    });
  });

  group('Dropdown', () {
    testWidgets('renders its label and selected value',
        (WidgetTester tester) async {
      await tester.pumpWidget(
        wrapWithApp(
          Dropdown<String>(
            label: 'Zone',
            value: 'zone-b',
            onChanged: (_) {},
            items: const {
              'zone-a': 'Zone A',
              'zone-b': 'Zone B',
            },
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Zone'), findsOneWidget);
      expect(
        tester
            .widget<DropdownButton<String>>(find.byType(DropdownButton<String>))
            .value,
        'zone-b',
      );
    });

    testWidgets('fires onChanged when enabled', (WidgetTester tester) async {
      var selected = 'zone-a';
      String? changedValue;

      await tester.pumpWidget(
        wrapWithApp(
          StatefulBuilder(
            builder: (context, setState) {
              return Dropdown<String>(
                label: 'Zone',
                value: selected,
                onChanged: (value) {
                  changedValue = value;
                  setState(() => selected = value!);
                },
                items: const {
                  'zone-a': 'Zone A',
                  'zone-b': 'Zone B',
                },
              );
            },
          ),
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.byType(DropdownButton<String>));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Zone B').last);
      await tester.pumpAndSettle();

      expect(changedValue, 'zone-b');
      expect(
        tester
            .widget<DropdownButton<String>>(find.byType(DropdownButton<String>))
            .value,
        'zone-b',
      );
    });

    testWidgets('disables the dropdown button when enabled is false',
        (WidgetTester tester) async {
      var changed = false;

      await tester.pumpWidget(
        wrapWithApp(
          Dropdown<String>(
            label: 'Zone',
            value: 'zone-a',
            enabled: false,
            onChanged: (_) => changed = true,
            items: const {
              'zone-a': 'Zone A',
              'zone-b': 'Zone B',
            },
          ),
        ),
      );
      await tester.pumpAndSettle();

      final dropdown = tester
          .widget<DropdownButton<String>>(find.byType(DropdownButton<String>));
      expect(dropdown.onChanged, isNull);
      await tester.tap(find.byType(DropdownButton<String>));
      await tester.pumpAndSettle();
      expect(changed, isFalse);
    });
  });
}
