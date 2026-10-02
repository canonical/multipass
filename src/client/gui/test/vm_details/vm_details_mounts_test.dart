import 'package:extended_text/extended_text.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart' show vmInfoProvider;
import 'package:multipass_gui/vm_details/vm_details_mounts.dart';

final _l10n = AppLocalizationsEn();

Widget _buildApp({
  required Widget child,
  required DetailedInfoItem vmInfo,
  String vmName = 'test-vm',
}) {
  return ProviderScope(
    overrides: [
      vmInfoProvider(vmName).overrideWithBuild((ref, _) => vmInfo),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: SizedBox(
          width: 600,
          child: child,
        ),
      ),
    ),
  );
}

DetailedInfoItem _itemWithMounts(List<MountPaths> mounts) {
  return DetailedInfoItem(
    instanceStatus: InstanceStatus(status: Status.STOPPED),
    mountInfo: MountInfo(mountPaths: mounts),
  );
}

void main() {
  const vmName = 'test-vm';

  group('MountDetails — idle phase with no mounts', () {
    testWidgets('shows Add Mount button', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.mountsAddMount), findsOneWidget);
    });

    testWidgets('does not show Configure button', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.commonConfigure), findsNothing);
    });

    testWidgets('shows Mounts title', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.mountsTitle), findsOneWidget);
    });
  });

  group('MountDetails — idle phase with existing mounts', () {
    final existingMount = MountPaths(
      sourcePath: '/home/user/projects',
      targetPath: '/home/ubuntu/projects',
    );

    testWidgets('shows Configure button', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.commonConfigure), findsOneWidget);
    });

    testWidgets('does not show Add Mount button initially', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.mountsAddMount), findsNothing);
    });

    testWidgets('source path is displayed', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(
        find.byWidgetPredicate(
          (w) => w is ExtendedText && w.data == '/home/user/projects',
        ),
        findsOneWidget,
      );
    });

    testWidgets('target path is displayed', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(
        find.byWidgetPredicate(
          (w) => w is ExtendedText && w.data == '/home/ubuntu/projects',
        ),
        findsOneWidget,
      );
    });

    testWidgets('column headers HOST DIRECTORY and GUEST DIRECTORY shown',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.mountHostDirLabel), findsOneWidget);
      expect(find.text(_l10n.mountGuestDirLabel), findsOneWidget);
    });
  });

  group('MountDetails — phase transitions', () {
    testWidgets('tapping Configure then Cancel returns to Configure',
        (tester) async {
      final existingMount = MountPaths(
        sourcePath: '/home/user/projects',
        targetPath: '/home/ubuntu/projects',
      );

      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.commonConfigure));
      await tester.pumpAndSettle();

      expect(find.text(_l10n.commonCancel), findsOneWidget);
      expect(find.text(_l10n.commonConfigure), findsNothing);

      await tester.tap(find.text(_l10n.commonCancel));
      await tester.pumpAndSettle();

      expect(find.text(_l10n.commonConfigure), findsOneWidget);
      expect(find.text(_l10n.commonCancel), findsNothing);
    });

    testWidgets('tapping Configure shows Add Mount button', (tester) async {
      final existingMount = MountPaths(
        sourcePath: '/home/user/projects',
        targetPath: '/home/ubuntu/projects',
      );

      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.commonConfigure));
      await tester.pumpAndSettle();

      expect(find.text(_l10n.mountsAddMount), findsOneWidget);
    });
  });
}
