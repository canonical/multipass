import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Daemon unavailable overlay appears and disappears',
      (tester) async {
    final mockDaemon = MockDaemon.nice();

    // Info fails → vmInfosStreamProvider enters error state →
    // daemonAvailableProvider = false → overlay shown.
    mockDaemon.onInfo =
        (_) => Stream.error(GrpcError.unavailable('daemon not available'));

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    // overrideDaemonAvailable: false — use real daemonAvailableProvider logic
    // (driven by vmInfosStreamProvider) instead of the always-true stub, so
    // the overlay can actually appear and disappear during the test.
    await launchApp(
      tester,
      [grpcClientProvider.overrideWithValue(mockDaemon.client)],
      overrideDaemonAvailable: false,
    );
    // Render the initial frame — do NOT call pumpAndSettle here because the
    // daemon-error overlay (with its spinner) may appear immediately and the
    // spinner keeps scheduling frames, causing pumpAndSettle to hang.
    await tester.pump();

    // Wait for the info poller to fire and pick up the error.
    // settle: false — the overlay contains a spinner that keeps scheduling
    // frames, so pumpAndSettle would never settle.
    await pumpUntil(
      tester,
      find.text('Waiting for daemon...'),
      settle: false,
    );

    // Overlay is visible in the "waiting" variant (ffi available → spinner).
    expect(find.text('Waiting for daemon...'), findsOneWidget);

    // Restore the daemon — next poll should make the overlay disappear.
    mockDaemon.onInfo = (_) => Stream.value(InfoReply());

    await pumpUntilGone(tester, find.text('Waiting for daemon...'));

    // Overlay gone.
    expect(find.text('Waiting for daemon...'), findsNothing);
  });
}
