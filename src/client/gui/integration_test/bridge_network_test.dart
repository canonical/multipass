import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets(
      'Virtualization settings: bridged network selection calls set RPC',
      (tester) async {
    const testNetwork = 'en0';

    final mockDaemon = MockDaemon.nice();

    mockDaemon.onInfo = (_) => Stream.value(InfoReply());

    // Return 'qemu' for driver, empty for bridged network, empty for others
    mockDaemon.onGet = (req) {
      final value = req.key == driverKey ? 'qemu' : '';
      return Stream.value(GetReply(value: value));
    };

    // Return one available network
    mockDaemon.onNetworks = (_) => Stream.value(
          NetworksReply(interfaces: [NetInterface(name: testNetwork)]),
        );

    mockDaemon.onVersion = (_) => Stream.value(VersionReply());
    mockDaemon.onDaemonInfo = (_) => Stream.value(DaemonInfoReply());

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Navigate to Settings
    await tester.tap(find.textContaining('Settings'));
    await pumpUntil(tester, find.text('Virtualization'));

    // Virtualization section is visible
    expect(find.text('Virtualization'), findsOneWidget);

    // Scroll to the Bridged network dropdown
    final bridgeDropdown = find.byType(DropdownButton<String>).last;
    await tester.ensureVisible(bridgeDropdown);
    await tester.pumpAndSettle();

    // "Bridged network" label is shown
    expect(find.textContaining('Bridged'), findsWidgets);

    // Dropdown shows "None" (no network currently selected)
    expect(find.text('None'), findsWidgets);

    // Capture the set request for bridged network
    SetRequest? capturedSetReq;
    mockDaemon.enqueueSet((req) {
      capturedSetReq = req;
      return Stream.value(SetReply());
    });

    // Open the Bridged network dropdown
    await tester.tap(bridgeDropdown);
    await tester.pumpAndSettle();

    // Select the network from the overlay
    await tester.tap(find.text(testNetwork).last);
    await tester.pumpAndSettle();

    // Verify set was called with bridgedNetworkKey and the network name
    expect(capturedSetReq?.key, equals(bridgedNetworkKey));
    expect(capturedSetReq?.val, equals(testNetwork));
    mockDaemon.assertAllConsumed();
  });
}
