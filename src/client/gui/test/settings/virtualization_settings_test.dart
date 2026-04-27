import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/settings/virtualization_settings.dart';

Widget _buildApp({
  String driver = 'qemu',
  String bridgedNetwork = '',
  Set<String> networks = const {},
}) {
  return ProviderScope(
    overrides: [
      driverProvider.overrideWithBuild((ref, _) => driver),
      bridgedNetworkProvider.overrideWithBuild((ref, _) => bridgedNetwork),
      networksProvider.overrideWith((_) async => BuiltSet<String>(networks)),
    ],
    child: MaterialApp(
      locale: const Locale('en'),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const Scaffold(
        body: SizedBox(width: 600, child: VirtualizationSettings()),
      ),
    ),
  );
}

void main() {
  group('VirtualizationSettings', () {
    testWidgets('shows the Virtualization section title', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pumpAndSettle();

      expect(find.text('Virtualization'), findsOneWidget);
    });

    testWidgets('shows the Driver label and dropdown', (tester) async {
      await tester.pumpWidget(_buildApp(driver: 'qemu'));
      await tester.pumpAndSettle();

      expect(find.text('Driver'), findsOneWidget);
      expect(find.byType(DropdownButton<String>), findsOneWidget);
    });

    testWidgets('hides bridge dropdown when no networks are available',
        (tester) async {
      await tester.pumpWidget(_buildApp(networks: {}));
      await tester.pumpAndSettle();

      expect(find.text('Bridged network'), findsNothing);
      expect(find.byType(DropdownButton<String>), findsOneWidget);
    });

    testWidgets('shows bridge dropdown when networks are available',
        (tester) async {
      await tester.pumpWidget(_buildApp(networks: {'eth0', 'en0'}));
      await tester.pumpAndSettle();

      expect(find.text('Bridged network'), findsOneWidget);
      expect(find.byType(DropdownButton<String>), findsNWidgets(2));
    });

    testWidgets(
        'bridge dropdown value is "none" when bridged network is not in the networks list',
        (tester) async {
      await tester.pumpWidget(_buildApp(
        bridgedNetwork: 'eth99',
        networks: {'eth0', 'en0'},
      ));
      await tester.pumpAndSettle();

      expect(find.text('None'), findsOneWidget);
    });
  });
}
