import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/main.dart' as app;
import 'package:multipass_gui/providers.dart';

import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Catalogue: images rendered from find reply', (tester) async {
    final mockDaemon = MockDaemon.nice();

    // Baseline: no VMs
    mockDaemon.onInfo = (_) => Stream.value(InfoReply());

    // Return three images: Ubuntu, Debian, Fedora
    mockDaemon.enqueueFind((_) => Stream.value(
          FindReply(
            imagesInfo: [
              FindReply_ImageInfo(
                os: 'Ubuntu',
                release: '22.04',
                codename: 'jammy',
                aliases: ['lts', '22.04'],
              ),
              FindReply_ImageInfo(
                os: 'Debian',
                release: 'bookworm',
                codename: 'bookworm',
                aliases: ['debian'],
              ),
              FindReply_ImageInfo(
                os: 'Fedora',
                release: '39',
                codename: '39',
                aliases: ['fedora'],
              ),
            ],
          ),
        ));

    // Start in-process gRPC server
    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    // Launch app - defaults to catalogue screen
    await tester.runAsync(() async {
      await app.main([
        ffiAvailableProvider.overrideWithValue(false),
        grpcClientProvider.overrideWithValue(mockDaemon.client),
      ]);
    });
    await tester.pumpAndSettle();

    // Catalogue welcome title visible
    expect(find.text('Welcome to Multipass'), findsOneWidget);

    // Wait for find to complete and images to render
    await tester.pump(const Duration(seconds: 3));
    await tester.pumpAndSettle();

    // Image cards rendered for each OS
    expect(find.text('Ubuntu Server'), findsOneWidget);
    expect(find.text('Debian'), findsOneWidget);
    expect(find.text('Fedora'), findsOneWidget);

    mockDaemon.assertAllConsumed();
  });
}
