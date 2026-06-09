import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/tooltip.dart';
import 'package:multipass_gui/vm_details/vm_status_icon.dart';

final _l10n = AppLocalizationsEn();

Widget buildWidget(Status status, {required bool isLaunching}) {
  return MaterialApp(
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

String statusLabel(WidgetTester tester) =>
    tester.widget<Tooltip>(find.byType(Tooltip)).message;

void main() {
  group('VmStatusIcon', () {
    for (final status in Status.values) {
      testWidgets('shows a non-empty label for ${status.name}', (tester) async {
        await tester.pumpWidget(buildWidget(status, isLaunching: false));

        expect(statusLabel(tester), isNotEmpty);
      });
    }

    testWidgets('shows the icon mapped to the status', (tester) async {
      await tester.pumpWidget(buildWidget(Status.RUNNING, isLaunching: false));

      expect(find.byWidget(icons[Status.RUNNING]!), findsOneWidget);
      expect(find.byType(CircularProgressIndicator), findsNothing);
    });

    testWidgets('falls back to unknownIcon for an unmapped status',
        (tester) async {
      await tester.pumpWidget(buildWidget(Status.UNKNOWN, isLaunching: false));

      expect(find.byWidget(unknownIcon), findsOneWidget);
    });

    testWidgets('shows a spinner and launching label instead of the status',
        (tester) async {
      await tester.pumpWidget(buildWidget(Status.RUNNING, isLaunching: true));

      expect(find.byType(CircularProgressIndicator), findsOneWidget);
      expect(find.byWidget(icons[Status.RUNNING]!), findsNothing);
      expect(statusLabel(tester), _l10n.vmStatusLabel('launching'));
    });
  });
}
