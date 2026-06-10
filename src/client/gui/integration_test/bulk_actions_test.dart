import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Bulk actions: select all, stop, and delete VMs', (tester) async {
    const vm1 = 'alpha-vm';
    const vm2 = 'beta-vm';

    final mockDaemon = MockDaemon.nice();

    InfoReply infoWithTwoRunning() => InfoReply(
          details: [
            DetailedInfoItem(
              name: vm1,
              instanceStatus: InstanceStatus(status: Status.RUNNING),
            ),
            DetailedInfoItem(
              name: vm2,
              instanceStatus: InstanceStatus(status: Status.RUNNING),
            ),
          ],
        );

    InfoReply infoWithTwoStopped() => InfoReply(
          details: [
            DetailedInfoItem(
              name: vm1,
              instanceStatus: InstanceStatus(status: Status.STOPPED),
            ),
            DetailedInfoItem(
              name: vm2,
              instanceStatus: InstanceStatus(status: Status.STOPPED),
            ),
          ],
        );

    mockDaemon.onInfo = (_) => Stream.value(infoWithTwoRunning());

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Navigate to Instances (VM table)
    await tester.tap(find.textContaining('Instances'));
    await pumpUntil(tester, find.byTooltip(vm1));

    // Both VMs should be listed (found by their name tooltips)
    expect(find.byTooltip(vm1), findsOneWidget);
    expect(find.byTooltip(vm2), findsOneWidget);

    // Select all VMs via the header checkbox
    await tester.tap(find.byType(Checkbox).first);
    await tester.pumpAndSettle();

    expect(find.text('Stop'), findsOneWidget);

    // Capture the stop request
    StopRequest? capturedStopReq;
    mockDaemon.enqueueStop((req) {
      capturedStopReq = req;
      mockDaemon.onInfo = (_) => Stream.value(infoWithTwoStopped());
      return Stream.value(StopReply());
    });

    await tester.tap(find.text('Stop'));
    await pumpUntil(tester, find.text('Stopped'));

    // Verify both VM names were included in the stop request
    final stoppedNames = capturedStopReq?.instanceNames.instanceName ?? [];
    expect(stoppedNames, containsAll([vm1, vm2]));

    // Both VMs are still selected; stopped VMs expose a Delete action
    expect(find.text('Delete'), findsOneWidget);

    // Capture the delete request; return empty info to clear the table
    DeleteRequest? capturedDeleteReq;
    mockDaemon.enqueueDelete((req) {
      capturedDeleteReq = req;
      mockDaemon.onInfo = (_) => Stream.value(InfoReply());
      return Stream.value(DeleteReply());
    });

    await tester.tap(find.text('Delete'));
    await tester.pumpAndSettle();

    // Confirmation dialog for multiple instances
    expect(find.text('Delete instances'), findsOneWidget);
    await tester.tap(find.text('Delete').last);
    await pumpUntilGone(tester, find.byTooltip(vm1));

    // Both VMs gone; purge flag was set
    expect(find.byTooltip(vm1), findsNothing);
    expect(find.byTooltip(vm2), findsNothing);
    expect(capturedDeleteReq?.purge, isTrue);

    mockDaemon.assertAllConsumed();
  });
}
