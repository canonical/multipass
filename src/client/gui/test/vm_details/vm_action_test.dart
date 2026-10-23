import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/vm_action.dart';

final _l10n = AppLocalizationsEn();

Widget buildWidget({required List<Status> statuses, VoidCallback? onPressed}) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: VmActionButton(
        action: VmAction.stop,
        currentStatuses: statuses,
        function: onPressed ?? () {},
      ),
    ),
  );
}

void main() {
  group('VmActionButton', () {
    testWidgets('shows the action label', (tester) async {
      await tester.pumpWidget(buildWidget(statuses: [Status.RUNNING]));

      expect(find.text(VmAction.stop.label(_l10n)), findsOneWidget);
    });

    for (final (statuses, enabled) in [
      ([Status.RUNNING], true),
      ([Status.STOPPED], false),
      ([Status.STOPPED, Status.RUNNING], true),
      (<Status>[], false),
    ]) {
      testWidgets(
          '${enabled ? 'invokes' : 'does not invoke'} stop when tapped with '
          'statuses ${statuses.map((s) => s.name)}', (tester) async {
        var pressed = false;
        await tester.pumpWidget(
          buildWidget(statuses: statuses, onPressed: () => pressed = true),
        );

        await tester.tap(find.byType(OutlinedButton));

        expect(pressed, enabled);
      });
    }
  });
}
