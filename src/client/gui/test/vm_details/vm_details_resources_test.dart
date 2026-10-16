import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details_resources.dart';

const _vmName = 'test-vm';

final _l10n = AppLocalizationsEn();

Widget _buildApp({Status status = Status.STOPPED}) {
  final cpusKey = (name: _vmName, resource: VmResource.cpus);
  final ramKey = (name: _vmName, resource: VmResource.memory);
  final diskKey = (name: _vmName, resource: VmResource.disk);

  return ProviderScope(
    overrides: [
      vmInfoProvider(_vmName).overrideWithBuild(
        (ref, _) => DetailedInfoItem(
          instanceStatus: InstanceStatus(status: status),
        ),
      ),
      vmResourceProvider(cpusKey).overrideWithBuild((ref, _) => '4'),
      // Loaded memory and disk values are formatted via FFI, which is
      // unavailable in unit tests.
      vmResourceProvider(ramKey).overrideWithBuild(
        (ref, _) => Completer<String>().future,
      ),
      vmResourceProvider(diskKey).overrideWithBuild(
        (ref, _) => Completer<String>().future,
      ),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const Scaffold(
        body: SizedBox(width: 800, child: ResourcesDetails(_vmName)),
      ),
    ),
  );
}

OutlinedButton _configureButton(WidgetTester tester) => tester.widget(
      find.widgetWithText(OutlinedButton, _l10n.commonConfigure),
    );

void main() {
  group('ResourcesDetails', () {
    testWidgets('shows the loaded CPU count', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(find.text(_l10n.resourcesCpusDisplay('4')), findsOneWidget);
    });

    testWidgets('shows placeholders while memory and disk are loading',
        (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(find.text(_l10n.resourcesMemoryDisplay('…')), findsOneWidget);
      expect(find.text(_l10n.resourcesDiskDisplay('…')), findsOneWidget);
    });

    testWidgets('enables Configure when status is STOPPED', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(_configureButton(tester).onPressed, isNotNull);
    });

    for (final status in [Status.RUNNING, Status.SUSPENDED, Status.STARTING]) {
      testWidgets('disables Configure when status is ${status.name}',
          (tester) async {
        await tester.pumpWidget(_buildApp(status: status));
        await tester.pump();

        expect(_configureButton(tester).onPressed, isNull);
      });
    }
  });
}
