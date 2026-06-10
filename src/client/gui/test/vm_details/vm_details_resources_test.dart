import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details_resources.dart';

const _vmName = 'test-vm';

Widget _buildApp({Status status = Status.STOPPED}) {
  final vmInfo = DetailedInfoItem(
    name: _vmName,
    instanceStatus: InstanceStatus(status: status),
  );
  final cpusKey = (name: _vmName, resource: VmResource.cpus);
  final ramKey = (name: _vmName, resource: VmResource.memory);
  final diskKey = (name: _vmName, resource: VmResource.disk);

  return ProviderScope(
    overrides: [
      vmInfoProvider(_vmName).overrideWithBuild((ref, _) => vmInfo),
      vmResourceProvider(cpusKey).overrideWithBuild((ref, _) => '4'),
      // Keep memory/disk in perpetual loading state so humanReadableMemory
      // is never called.
      vmResourceProvider(ramKey).overrideWithBuild(
        (ref, _) => Completer<String>().future,
      ),
      vmResourceProvider(diskKey).overrideWithBuild(
        (ref, _) => Completer<String>().future,
      ),
    ],
    child: MaterialApp(
      locale: const Locale('en'),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: SizedBox(
          width: 800,
          child: ResourcesDetails(_vmName),
        ),
      ),
    ),
  );
}

void main() {
  group('ResourcesDetails', () {
    testWidgets('shows the Resources title', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(find.text('Resources'), findsOneWidget);
    });

    testWidgets('shows CPUs value from provider data', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(find.text('CPUs 4'), findsOneWidget);
    });

    testWidgets('shows "…" placeholders while memory and disk are loading',
        (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(find.textContaining('…'), findsWidgets);
    });

    testWidgets('Configure button is enabled when VM is stopped',
        (tester) async {
      await tester.pumpWidget(_buildApp(status: Status.STOPPED));
      await tester.pump();

      final button = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Configure'),
      );
      expect(button.onPressed, isNotNull);
    });

    testWidgets('Configure button is disabled when VM is running',
        (tester) async {
      await tester.pumpWidget(_buildApp(status: Status.RUNNING));
      await tester.pump();

      final button = tester.widget<OutlinedButton>(
        find.widgetWithText(OutlinedButton, 'Configure'),
      );
      expect(button.onPressed, isNull);
    });
  });
}
