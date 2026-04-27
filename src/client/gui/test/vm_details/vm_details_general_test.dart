import 'package:fixnum/fixnum.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:intl/intl.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details_general.dart';
import 'package:protobuf/well_known_types/google/protobuf/timestamp.pb.dart';

void main() {
  group('InstanceDetailsExtensions.formattedCreationTime', () {
    test('returns "-" when isLaunching is true and timestamp is zero', () {
      final details = InstanceDetails(
        creationTimestamp: Timestamp(seconds: Int64(0), nanos: 0),
      );

      expect(details.formattedCreationTime(isLaunching: true), equals('-'));
    });

    test(
        'returns formatted datetime when isLaunching is true and timestamp is non-zero',
        () {
      final dt = DateTime.utc(2024, 1, 15, 10, 30, 0);
      final seconds = dt.millisecondsSinceEpoch ~/ 1000;
      final ts = Timestamp(seconds: Int64(seconds), nanos: 0);
      final details = InstanceDetails(creationTimestamp: ts);

      final result = details.formattedCreationTime(isLaunching: true);

      final expected =
          DateFormat('yyyy-MM-dd HH:mm:ss').format(ts.toDateTime());
      expect(result, equals(expected));
      expect(result, isNot(equals('-')));
    });

    test(
        'returns formatted datetime when isLaunching is false with a real timestamp',
        () {
      final dt = DateTime.utc(2024, 1, 15, 10, 30, 0);
      final seconds = dt.millisecondsSinceEpoch ~/ 1000;
      final ts = Timestamp(seconds: Int64(seconds), nanos: 0);
      final details = InstanceDetails(creationTimestamp: ts);

      final result = details.formattedCreationTime(isLaunching: false);

      final expected =
          DateFormat('yyyy-MM-dd HH:mm:ss').format(ts.toDateTime());
      expect(result, equals(expected));
    });

    test('uses default zero timestamp when creationTimestamp is not set', () {
      final details = InstanceDetails();

      expect(details.formattedCreationTime(isLaunching: true), equals('-'));
    });
  });

  group('VmStat', () {
    testWidgets('renders the label text', (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: const VmStat(
            label: 'CPU Usage',
            width: 100,
            height: 35,
            child: SizedBox.shrink(),
          ),
        ),
      );

      expect(find.text('CPU Usage'), findsOneWidget);
    });

    testWidgets('renders the child widget', (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: const VmStat(
            label: '',
            width: 100,
            height: 35,
            child: Text('child content'),
          ),
        ),
      );

      expect(find.text('child content'), findsOneWidget);
    });
  });

  group('GeneralDetails', () {
    Widget buildWidget({
      List<String> ipv4 = const [],
      String uptime = '',
      bool isLaunching = false,
    }) {
      return ProviderScope(
        overrides: [
          vmInfoProvider('test-vm').overrideWithBuild(
            (ref, notifier) => DetailedInfoItem(
              instanceInfo: InstanceDetails(
                ipv4: ipv4,
                uptime: uptime,
              ),
            ),
          ),
          isLaunchingProvider.overrideWith((ref, _) => isLaunching),
        ],
        child: MaterialApp(
          locale: const Locale('en'),
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          supportedLocales: AppLocalizations.supportedLocales,
          home: const Scaffold(body: GeneralDetails('test-vm')),
        ),
      );
    }

    testWidgets('shows first IPv4 address as private IP', (tester) async {
      await tester.pumpWidget(buildWidget(ipv4: ['192.168.64.1', '10.0.0.1']));
      await tester.pumpAndSettle();

      expect(find.text('192.168.64.1'), findsOneWidget);
    });

    testWidgets('shows second IPv4 address as public IP', (tester) async {
      await tester.pumpWidget(buildWidget(ipv4: ['192.168.64.1', '10.0.0.1']));
      await tester.pumpAndSettle();

      expect(find.text('10.0.0.1'), findsOneWidget);
    });

    testWidgets('shows "-" for public IP when only one IPv4 address',
        (tester) async {
      await tester.pumpWidget(buildWidget(ipv4: ['192.168.64.1']));
      await tester.pumpAndSettle();

      expect(find.text('192.168.64.1'), findsOneWidget);
      // The public IP falls back to '-' since there is no second address.
      expect(find.text('-'), findsWidgets);
    });

    testWidgets('shows "-" for both IPs when ipv4 list is empty',
        (tester) async {
      await tester.pumpWidget(buildWidget());
      await tester.pumpAndSettle();

      expect(find.text('-'), findsWidgets);
    });

    testWidgets('renders the uptime text', (tester) async {
      await tester.pumpWidget(buildWidget(uptime: '1 hour, 23 minutes'));
      await tester.pumpAndSettle();

      expect(find.text('1 hour, 23 minutes'), findsOneWidget);
    });
  });
}
