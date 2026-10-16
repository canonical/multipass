import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details_general.dart';
import 'package:protobuf/well_known_types/google/protobuf/timestamp.pb.dart';

const _vmName = 'test-vm';

final _l10n = AppLocalizationsEn();

Widget _buildApp({List<String> ipv4 = const [], bool isLaunching = false}) {
  return ProviderScope(
    overrides: [
      vmInfoProvider(_vmName).overrideWithBuild(
        (ref, _) => DetailedInfoItem(
          instanceInfo: InstanceDetails(ipv4: ipv4),
        ),
      ),
      isLaunchingProvider.overrideWith((ref, _) => isLaunching),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const Scaffold(body: GeneralDetails(_vmName)),
    ),
  );
}

Finder _statValue(String label, String value) => find.descendant(
      of: find.widgetWithText(VmStat, label),
      matching: find.text(value),
    );

void main() {
  group('InstanceDetailsExtensions.formattedCreationTime', () {
    test('returns "-" while launching without a timestamp', () {
      expect(InstanceDetails().formattedCreationTime(isLaunching: true), '-');
    });

    for (final isLaunching in [true, false]) {
      test(
          'formats a set timestamp in local time when '
          'isLaunching=$isLaunching', () {
        final details = InstanceDetails(
          creationTimestamp:
              Timestamp.fromDateTime(DateTime(2024, 1, 15, 10, 30, 5)),
        );

        expect(
          details.formattedCreationTime(isLaunching: isLaunching),
          '2024-01-15 10:30:05',
        );
      });
    }
  });

  group('VmStat', () {
    testWidgets('scales its size with the text scale factor', (tester) async {
      await tester.pumpWidget(
        const MaterialApp(
          home: Center(
            child: MediaQuery(
              data: MediaQueryData(textScaler: TextScaler.linear(2)),
              child: VmStat(
                label: 'label',
                width: 100,
                height: 35,
                child: SizedBox.shrink(),
              ),
            ),
          ),
        ),
      );

      expect(tester.getSize(find.byType(VmStat)), const Size(200, 70));
    });
  });

  group('GeneralDetails', () {
    testWidgets('shows the first IPv4 as private and the second as public',
        (tester) async {
      await tester.pumpWidget(_buildApp(ipv4: ['192.168.64.1', '10.0.0.1']));
      await tester.pumpAndSettle();

      expect(_statValue(_l10n.vmStatPrivateIp, '192.168.64.1'), findsOneWidget);
      expect(_statValue(_l10n.vmStatPublicIp, '10.0.0.1'), findsOneWidget);
    });

    testWidgets('shows "-" for the public IP with a single address',
        (tester) async {
      await tester.pumpWidget(_buildApp(ipv4: ['192.168.64.1']));
      await tester.pumpAndSettle();

      expect(_statValue(_l10n.vmStatPrivateIp, '192.168.64.1'), findsOneWidget);
      expect(_statValue(_l10n.vmStatPublicIp, '-'), findsOneWidget);
    });

    testWidgets('shows "-" for both IPs without addresses', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pumpAndSettle();

      expect(_statValue(_l10n.vmStatPrivateIp, '-'), findsOneWidget);
      expect(_statValue(_l10n.vmStatPublicIp, '-'), findsOneWidget);
    });

    testWidgets('shows "-" as the creation time while launching',
        (tester) async {
      await tester.pumpWidget(_buildApp(isLaunching: true));
      // The launching spinner never settles.
      await tester.pump();

      expect(_statValue(_l10n.vmStatCreated, '-'), findsOneWidget);
    });
  });
}
