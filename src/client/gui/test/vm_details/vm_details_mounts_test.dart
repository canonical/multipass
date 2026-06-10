import 'package:extended_text/extended_text.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart' show vmInfoProvider;
import 'package:multipass_gui/vm_details/vm_details_mounts.dart';

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
      locale: const Locale('en'),
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

      expect(find.text('Add mount'), findsOneWidget);
    });

    testWidgets('does not show Configure button', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Configure'), findsNothing);
    });

    testWidgets('shows Mounts title', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Mounts'), findsOneWidget);
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

      expect(find.text('Configure'), findsOneWidget);
    });

    testWidgets('does not show Add Mount button initially', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          vmInfo: _itemWithMounts([existingMount]),
          child: const MountDetails(vmName),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Add mount'), findsNothing);
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

      expect(find.text('HOST DIRECTORY'), findsOneWidget);
      expect(find.text('GUEST DIRECTORY'), findsOneWidget);
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

      await tester.tap(find.text('Configure'));
      await tester.pumpAndSettle();

      expect(find.text('Cancel'), findsOneWidget);
      expect(find.text('Configure'), findsNothing);

      await tester.tap(find.text('Cancel'));
      await tester.pumpAndSettle();

      expect(find.text('Configure'), findsOneWidget);
      expect(find.text('Cancel'), findsNothing);
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

      await tester.tap(find.text('Configure'));
      await tester.pumpAndSettle();

      expect(find.text('Add mount'), findsOneWidget);
    });
  });
}
