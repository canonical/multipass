import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Empty state: message contains link to Catalogue',
      (tester) async {
    final mockDaemon = MockDaemon.nice();
    mockDaemon.onInfo = (_) => Stream.value(InfoReply());

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    await tester.tap(find.textContaining('Instances'));
    await tester.pumpAndSettle();
    await tester.pump(const Duration(seconds: 3));
    await tester.pumpAndSettle();

    expect(find.textContaining('Return to the Catalogue'), findsOneWidget);
  });
}
