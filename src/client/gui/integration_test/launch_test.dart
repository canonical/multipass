import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Launch: image shown, tap Launch triggers daemon launch',
      (tester) async {
    final mockDaemon = MockDaemon.nice();

    mockDaemon.onInfo = (_) => Stream.value(InfoReply());

    const testAlias = 'lts';
    mockDaemon.enqueueFind((_) => Stream.value(
          FindReply(
            imagesInfo: [
              FindReply_ImageInfo(
                os: 'Ubuntu',
                release: '22.04',
                codename: 'jammy',
                aliases: [testAlias, '22.04'],
              ),
            ],
          ),
        ));

    // Capture the launch request to assert after pump
    LaunchRequest? capturedLaunchReq;
    mockDaemon.enqueueLaunch((req) {
      capturedLaunchReq = req;

      // After launch succeeds, info should return the running VM
      mockDaemon.onInfo = (_) => Stream.value(
            InfoReply(
              details: [
                DetailedInfoItem(
                  name: req.instanceName,
                  instanceStatus: InstanceStatus(status: Status.RUNNING),
                ),
              ],
            ),
          );

      return Stream.value(LaunchReply());
    });

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Catalogue shown
    expect(find.text('Welcome to Multipass'), findsOneWidget);

    // Wait for find to complete and image cards to appear
    await pumpUntil(tester, find.text('Ubuntu Server'));

    // Ubuntu Server card is rendered
    expect(find.text('Ubuntu Server'), findsOneWidget);

    // Tap the Launch button on the Ubuntu Server card.
    await tester.tap(find.text('Launch').first);

    // Wait for the launch to complete and the sidebar badge to update
    await pumpUntil(tester, find.text('1'));

    expect(capturedLaunchReq?.image, equals(testAlias));

    // Sidebar Instances badge shows count of 1
    expect(find.text('1'), findsOneWidget);

    // Navigate to Instances page and verify the launched VM appears
    await tester.tap(find.textContaining('Instances'));
    await pumpUntil(tester, find.text('All Instances'));

    expect(find.text('All Instances'), findsOneWidget);
    expect(find.byTooltip(capturedLaunchReq!.instanceName), findsOneWidget);

    mockDaemon.assertAllConsumed();
  });
}
