import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details_bridge.dart';

const _vmName = 'test-vm';

Widget _buildApp({
  Status status = Status.STOPPED,
  bool bridged = false,
  Set<String> networks = const {},
  String? bridgedNetworkSetting,
}) {
  final vmInfo = DetailedInfoItem(
    name: _vmName,
    instanceStatus: InstanceStatus(status: status),
  );
  final bridgeKey = (name: _vmName, resource: VmResource.bridged);

  return ProviderScope(
    overrides: [
      vmInfoProvider(_vmName).overrideWithBuild((ref, _) => vmInfo),
      vmResourceProvider(bridgeKey).overrideWithBuild(
        (ref, _) => bridged.toString(),
      ),
      networksProvider.overrideWith(
        (_) async => BuiltSet<String>(networks),
      ),
      daemonSettingProvider('local.bridged-network').overrideWithBuild(
        (ref, _) => bridgedNetworkSetting ?? '',
      ),
    ],
    child: MaterialApp(
      locale: const Locale('en'),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: SizedBox(
          width: 600,
          child: BridgedDetails(_vmName),
        ),
      ),
    ),
  );
}

void main() {
  group('BridgedDetails', () {
    testWidgets('shows "not connected" status text when not bridged',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(status: Status.STOPPED, bridged: false),
      );
      await tester.pumpAndSettle();

      expect(find.textContaining('not connected', findRichText: true),
          findsOneWidget);
    });

    testWidgets('shows "connected" status text when bridged', (tester) async {
      await tester.pumpWidget(
        _buildApp(status: Status.STOPPED, bridged: true),
      );
      await tester.pumpAndSettle();

      expect(
        find.text('Status: connected'),
        findsOneWidget,
      );
    });

    testWidgets('Configure button is disabled when VM is running',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(status: Status.RUNNING, bridged: false),
      );
      await tester.pumpAndSettle();

      final button = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Configure'),
      );
      expect(button.onPressed, isNull);
    });

    testWidgets('shows Bridge title', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pumpAndSettle();

      expect(find.textContaining('Bridge', findRichText: true), findsOneWidget);
    });

    testWidgets('tapping Configure enters editing mode', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          status: Status.STOPPED,
          bridged: false,
          networks: {'eth0'},
          bridgedNetworkSetting: 'eth0',
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.widgetWithText(OutlinedButton, 'Configure'));
      await tester.pumpAndSettle();

      // In editing mode, Cancel button is shown instead of Configure
      expect(find.widgetWithText(OutlinedButton, 'Cancel'), findsOneWidget);
      expect(find.widgetWithText(OutlinedButton, 'Configure'), findsNothing);
    });

    testWidgets('Cancel button exits editing mode', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          status: Status.STOPPED,
          bridged: false,
          networks: {'eth0'},
          bridgedNetworkSetting: 'eth0',
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.widgetWithText(OutlinedButton, 'Configure'));
      await tester.pumpAndSettle();

      await tester.tap(find.widgetWithText(OutlinedButton, 'Cancel'));
      await tester.pumpAndSettle();

      expect(find.widgetWithText(OutlinedButton, 'Configure'), findsOneWidget);
      expect(find.widgetWithText(OutlinedButton, 'Cancel'), findsNothing);
    });
  });
}
