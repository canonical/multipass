import 'package:flutter/material.dart';
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

    // Info fails -> daemonAvailableProvider = false -> overlay shown.
    // With ffiAvailableProvider = false the DaemonUnavailable widget shows the
    // "Fatal Error" variant (Icons.error + "Fatal Error" + "Exit Application").
    mockDaemon.onInfo =
        (_) => Stream.error(GrpcError.unavailable('daemon not available'));

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Wait for the info poller to fire and pick up the error.
    await pumpUntil(tester, find.text('Fatal Error'));

    // Overlay is visible in the "Fatal Error" variant.
    expect(find.byIcon(Icons.error), findsOneWidget);
    expect(find.text('Fatal Error'), findsOneWidget);
    expect(find.text('Exit Application'), findsOneWidget);

    // Restore the daemon - next poll should make the overlay disappear.
    mockDaemon.onInfo = (_) => Stream.value(InfoReply());

    await pumpUntilGone(tester, find.text('Fatal Error'));

    // DaemonUnavailable returns SizedBox.shrink() when available == true.
    expect(find.text('Fatal Error'), findsNothing);
    expect(find.byIcon(Icons.error), findsNothing);
  });
}
