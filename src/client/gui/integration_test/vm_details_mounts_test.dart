import 'dart:io' show Platform;

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('VM details: add mount form, cancel, then submit mount',
      (tester) async {
    const vmName = 'test-vm';

    final mockDaemon = MockDaemon.nice();

    // Running VM with no mounts
    mockDaemon.onInfo = (_) => Stream.value(InfoReply(
          details: [
            DetailedInfoItem(
              name: vmName,
              instanceStatus: InstanceStatus(status: Status.RUNNING),
            ),
          ],
        ));

    mockDaemon.onGet = (_) => Stream.value(GetReply(value: ''));
    mockDaemon.onNetworks = (_) => Stream.value(NetworksReply());
    mockDaemon.onVersion = (_) => Stream.value(VersionReply());
    mockDaemon.onDaemonInfo = (_) => Stream.value(DaemonInfoReply());

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Navigate to Instances then to VM detail
    await tester.tap(find.textContaining('Instances'));
    await pumpUntil(tester, find.byTooltip(vmName));

    // Navigate to VM detail
    await tester.tap(find.byTooltip(vmName));
    await tester.pumpAndSettle();

    // Switch to Details tab
    await tester.tap(find.text('Details'));
    await pumpUntil(tester, find.text('Add mount'));

    // Scroll to find the "Add mount" button (at bottom of details)
    final addMountFinder = find.text('Add mount');
    await tester.ensureVisible(addMountFinder);
    await tester.pumpAndSettle();

    // "Add mount" button is visible (no mounts exist in idle phase)
    expect(addMountFinder, findsOneWidget);

    // Tap "Add mount" to enter adding phase
    await tester.tap(addMountFinder);
    await tester.pumpAndSettle();

    // Mount form shows HOST DIRECTORY and GUEST DIRECTORY labels
    expect(find.text('HOST DIRECTORY'), findsOneWidget);
    expect(find.text('GUEST DIRECTORY'), findsOneWidget);

    // Cancel button appears
    expect(find.text('Cancel'), findsOneWidget);

    // Tap Cancel to go back to idle phase
    await tester.tap(find.text('Cancel'));
    await tester.pumpAndSettle();

    // Form headers gone, "Add mount" button is back
    expect(find.text('HOST DIRECTORY'), findsNothing);
    expect(find.text('Add mount'), findsOneWidget);

    // Tap "Add mount" again and this time submit
    await tester.tap(find.text('Add mount'));
    await tester.pumpAndSettle();

    // Capture the mount request; update info to include the new mount
    MountRequest? capturedMountReq;
    mockDaemon.enqueueMount((req) {
      capturedMountReq = req;
      mockDaemon.onInfo = (_) => Stream.value(InfoReply(
            details: [
              DetailedInfoItem(
                name: vmName,
                instanceStatus: InstanceStatus(status: Status.RUNNING),
                mountInfo: MountInfo(
                  mountPaths: [
                    MountPaths(
                      sourcePath: req.sourcePath,
                      targetPath: req.targetPaths.first.targetPath,
                    ),
                  ],
                ),
              ),
            ],
          ));
      return Stream.value(MountReply());
    });

    // The source field is pre-filled with the home directory; tap Save
    await tester.tap(find.text('Save'));
    await tester.pumpAndSettle();

    // Mount RPC was called with the home directory as source
    final expectedSource = Platform.environment['HOME'] ?? '';
    expect(capturedMountReq?.sourcePath, equals(expectedSource));
    expect(capturedMountReq?.targetPaths.first.instanceName, equals(vmName));

    // Wait for the info poller to deliver the updated reply with the mount
    // entry. Once visible, "Add mount" is replaced by "Configure".
    await pumpUntil(tester, find.text('Configure'));

    // Mount list headers are visible (MountPointsView shows them when
    // mounts is non-empty) and the add-mount button is gone.
    expect(find.text('HOST DIRECTORY'), findsOneWidget);
    expect(find.text('GUEST DIRECTORY'), findsOneWidget);
    expect(find.text('Add mount'), findsNothing);

    mockDaemon.assertAllConsumed();
  });
}
