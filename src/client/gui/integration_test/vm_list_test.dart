import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('VM list: "All Instances" heading shown with multiple VMs',
      (tester) async {
    const vm1 = 'alpha';
    const vm2 = 'bravo';
    const vm3 = 'charlie';

    final mockDaemon = MockDaemon.nice();
    mockDaemon.onInfo = (_) => Stream.value(InfoReply(
          details: [
            DetailedInfoItem(
              name: vm1,
              instanceStatus: InstanceStatus(status: Status.RUNNING),
            ),
            DetailedInfoItem(
              name: vm2,
              instanceStatus: InstanceStatus(status: Status.STOPPED),
            ),
            DetailedInfoItem(
              name: vm3,
              instanceStatus: InstanceStatus(status: Status.SUSPENDED),
            ),
          ],
        ));

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Navigate to the Instances screen
    await tester.tap(find.textContaining('Instances'));
    await pumpUntil(tester, find.text('All Instances'));

    // Heading is present
    expect(find.text('All Instances'), findsOneWidget);

    // All 3 VM names appear in the table (identified by tooltip)
    expect(find.byTooltip(vm1), findsOneWidget);
    expect(find.byTooltip(vm2), findsOneWidget);
    expect(find.byTooltip(vm3), findsOneWidget);
  });
}
