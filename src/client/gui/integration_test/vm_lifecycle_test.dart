import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('VM lifecycle: start, suspend, stop, delete', (tester) async {
    const vmName = 'test-vm';

    final mockDaemon = MockDaemon.nice();

    InfoReply infoWithVm(Status status) => InfoReply(
          details: [
            DetailedInfoItem(
              name: vmName,
              instanceStatus: InstanceStatus(status: status),
            ),
          ],
        );

    // 1. Baseline: VM in STOPPED state
    mockDaemon.onInfo = (_) => Stream.value(infoWithVm(Status.STOPPED));

    // 2. Start in-process gRPC server
    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    // 3. Launch the real app with mock overrides
    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Navigate to Instances (VM table)
    await tester.tap(find.textContaining('Instances'));
    await tester.pumpAndSettle();

    await pumpUntil(tester, find.byTooltip(vmName));

    // Select the VM via its row checkbox
    await tester.tap(find.byType(Checkbox).last);
    await tester.pumpAndSettle();

    // 4. Start VM
    StartRequest? capturedStartReq;
    mockDaemon.enqueueStart((req) {
      capturedStartReq = req;
      mockDaemon.onInfo = (_) => Stream.value(infoWithVm(Status.RUNNING));
      return Stream.value(StartReply());
    });
    await tester.tap(find.text('Start'));
    await pumpUntil(tester, find.text('Suspend'));
    expect(capturedStartReq?.instanceNames.instanceName, contains(vmName));
    expect(find.byTooltip(vmName), findsWidgets);

    // 5. Suspend
    SuspendRequest? capturedSuspendReq;
    mockDaemon.enqueueSuspend((req) {
      capturedSuspendReq = req;
      mockDaemon.onInfo = (_) => Stream.value(infoWithVm(Status.SUSPENDED));
      return Stream.value(SuspendReply());
    });
    await tester.tap(find.text('Suspend'));
    await pumpUntilGone(tester, find.text('Suspend'));
    expect(capturedSuspendReq?.instanceNames.instanceName, contains(vmName));

    // 6. Start VM again
    StartRequest? capturedStartReq2;
    mockDaemon.enqueueStart((req) {
      capturedStartReq2 = req;
      mockDaemon.onInfo = (_) => Stream.value(infoWithVm(Status.RUNNING));
      return Stream.value(StartReply());
    });
    await tester.tap(find.text('Start'));
    await pumpUntil(tester, find.text('Suspend'));
    expect(capturedStartReq2?.instanceNames.instanceName, contains(vmName));

    // 7. Stop VM
    StopRequest? capturedStopReq;
    mockDaemon.enqueueStop((req) {
      capturedStopReq = req;
      mockDaemon.onInfo = (_) => Stream.value(infoWithVm(Status.STOPPED));
      return Stream.value(StopReply());
    });
    await tester.tap(find.text('Stop'));
    await pumpUntilGone(tester, find.text('Stop'));
    expect(capturedStopReq?.instanceNames.instanceName, contains(vmName));

    // 8. Delete VM
    await tester.tap(find.text('Delete'));
    await tester.pumpAndSettle();

    // Confirmation dialog visible
    expect(find.text('Delete instance'), findsOneWidget);
    expect(find.text('Cancel'), findsOneWidget);

    // Cancel - no daemon call
    await tester.tap(find.text('Cancel'));
    await tester.pumpAndSettle();

    // VM still in table
    expect(find.byTooltip(vmName), findsWidgets);

    // 9. Delete: re-select, confirm this time
    // VM should still be selected since name hasn't changed
    DeleteRequest? capturedDeleteReq;
    mockDaemon.enqueueDelete((req) {
      capturedDeleteReq = req;
      mockDaemon.onInfo = (_) => Stream.value(InfoReply());
      return Stream.value(DeleteReply());
    });
    await tester.tap(find.text('Delete'));
    await tester.pumpAndSettle();

    expect(find.text('Delete instance'), findsOneWidget);

    // Confirm - last "Delete" text is the dialog's confirm button
    await tester.tap(find.text('Delete').last);
    await pumpUntilGone(tester, find.byTooltip(vmName));

    // VM gone from table; purge flag was set
    expect(find.byTooltip(vmName), findsNothing);
    expect(capturedDeleteReq?.purge, isTrue);

    mockDaemon.assertAllConsumed();
  });
}
