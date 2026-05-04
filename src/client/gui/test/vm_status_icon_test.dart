import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_details/vm_status_icon.dart';

Widget buildWidget(Status status, {required bool isLaunching}) {
  return MaterialApp(
    locale: const Locale('en'),
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: Row(
        children: [
          Expanded(child: VmStatusIcon(status, isLaunching: isLaunching)),
        ],
      ),
    ),
  );
}

void main() {
  group('VmStatusIcon', () {
    testWidgets('shows CircularProgressIndicator when isLaunching is true',
        (tester) async {
      await tester.pumpWidget(
        buildWidget(Status.RUNNING, isLaunching: true),
      );
      await tester.pump();

      expect(find.byType(CircularProgressIndicator), findsOneWidget);
    });

    testWidgets('does not show CircularProgressIndicator when not launching',
        (tester) async {
      await tester.pumpWidget(
        buildWidget(Status.RUNNING, isLaunching: false),
      );
      await tester.pump();

      expect(find.byType(CircularProgressIndicator), findsNothing);
    });
  });

  group('icons map', () {
    const cases = [
      (Status.RUNNING, Icons.circle),
      (Status.STOPPED, Icons.circle),
      (Status.SUSPENDED, Icons.circle),
      (Status.RESTARTING, Icons.more_horiz),
      (Status.STARTING, Icons.more_horiz),
      (Status.SUSPENDING, Icons.more_horiz),
      (Status.DELETED, Icons.close),
      (Status.DELAYED_SHUTDOWN, Icons.circle),
    ];

    for (final (status, expectedIcon) in cases) {
      test('has correct icon for ${status.name}', () {
        expect(icons.containsKey(status), isTrue);
        expect(icons[status]!.icon, equals(expectedIcon));
      });
    }
  });
}
