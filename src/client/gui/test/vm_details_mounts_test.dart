import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/mount_points.dart';
import 'package:multipass_gui/vm_details/vm_details_mounts.dart';

void main() {
  const vmName = 'test-vm';

  Widget buildWidget(Status status) {
    return ProviderScope(
      overrides: [
        vmInfoProvider(vmName).overrideWithBuild(
          (ref, notifier) => DetailedInfoItem(
            instanceStatus: InstanceStatus(status: status),
          ),
        ),
      ],
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: MountDetails(vmName),
        ),
      ),
    );
  }

  group('MountDetails add mount button', () {
    for (final status in [
      Status.RUNNING,
      Status.STOPPED,
      Status.SUSPENDED,
      Status.STARTING,
    ]) {
      testWidgets('is enabled when status is ${status.name}', (tester) async {
        await tester.pumpWidget(buildWidget(status));
        await tester.pumpAndSettle();

        final button = tester.widget<OutlinedButton>(
          find.byType(OutlinedButton),
        );
        expect(button.onPressed, isNotNull);
      });
    }

    testWidgets('is disabled when instance is unavailable', (tester) async {
      await tester.pumpWidget(buildWidget(Status.UNAVAILABLE));
      await tester.pumpAndSettle();

      final button = tester.widget<OutlinedButton>(
        find.byType(OutlinedButton),
      );
      expect(button.onPressed, isNull);
    });
  });

  group('MountDetails becoming unavailable mid-edit', () {
    testWidgets(
        'closes the add-mount form and disables actions when instance '
        'becomes unavailable while adding a mount', (tester) async {
      await tester.pumpWidget(buildWidget(Status.RUNNING));
      await tester.pumpAndSettle();

      // Open the add-mount form.
      await tester.tap(find.widgetWithText(OutlinedButton, 'Add mount'));
      await tester.pumpAndSettle();

      expect(find.byType(EditableMountPoint), findsOneWidget);
      expect(find.widgetWithText(TextButton, 'Save'), findsOneWidget);

      // Instance becomes unavailable (e.g. its zone was disabled).
      await tester.pumpWidget(buildWidget(Status.UNAVAILABLE));
      await tester.pumpAndSettle();

      // The form should close and no save action should remain available.
      expect(find.byType(EditableMountPoint), findsNothing);
      expect(find.widgetWithText(TextButton, 'Save'), findsNothing);

      // The top-level button reverts to a disabled Add mount button.
      final button = tester.widget<OutlinedButton>(
        find.byType(OutlinedButton),
      );
      expect(button.onPressed, isNull);
    });
  });
}
