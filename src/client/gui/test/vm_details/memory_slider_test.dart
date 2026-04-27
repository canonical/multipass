import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_details/memory_slider.dart';

String _fakeFormat(int bytes) => '${bytes}B';

Widget _buildApp({
  int min = 512,
  int max = 8192,
  int sysMax = 4096,
  int? initialValue,
  void Function(int?)? onSaved,
}) {
  return MaterialApp(
    locale: const Locale('en'),
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(
      body: SizedBox(
        width: 600,
        height: 400,
        child: Form(
          child: MemorySlider(
            label: 'Memory',
            min: min,
            max: max,
            sysMax: sysMax,
            initialValue: initialValue,
            onSaved: onSaved ?? (_) {},
            memoryFormatter: _fakeFormat,
          ),
        ),
      ),
    ),
  );
}

void main() {
  group('MemorySlider', () {
    testWidgets('shows min and max labels using the injected formatter',
        (tester) async {
      await tester.pumpWidget(_buildApp(min: 512, max: 8192));
      await tester.pump();

      expect(find.text('512B'), findsOneWidget);
      expect(find.text('8192B'), findsOneWidget);
    });

    testWidgets('shows over-provisioning warning when initialValue > sysMax',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(
          min: 512,
          max: 8000,
          sysMax: 4000,
          initialValue: 6000,
        ),
      );
      await tester.pump();

      expect(find.byIcon(Icons.warning_rounded), findsOneWidget);
    });

    testWidgets('does not show warning when initialValue <= sysMax',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(
          min: 512,
          max: 8000,
          sysMax: 4000,
          initialValue: 2000,
        ),
      );
      await tester.pump();

      expect(find.byIcon(Icons.warning_rounded), findsNothing);
    });

    testWidgets('unit dropdown is rendered', (tester) async {
      await tester.pumpWidget(_buildApp());
      await tester.pump();

      expect(
          find.byType(DropdownButton<(num Function(num), num Function(num))>),
          findsOneWidget);
    });

    testWidgets('slider is rendered with a valid initialValue', (tester) async {
      await tester.pumpWidget(
        _buildApp(
          min: 512,
          max: 8000,
          sysMax: 4000,
          initialValue: 2048,
        ),
      );
      await tester.pump();

      expect(find.byType(Slider), findsOneWidget);
    });
  });
}
