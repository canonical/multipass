import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/memory_usage.dart';

void main() {
  Widget buildWidget(String used, String total) {
    return MaterialApp(
      home: Scaffold(
        body: MemoryUsage(used: used, total: total),
      ),
    );
  }

  group('MemoryUsage color logic', () {
    for (final (used, colorName, color) in [
      ('500', 'normalColor', MemoryUsage.normalColor),
      ('799', 'normalColor', MemoryUsage.normalColor),
      ('800', 'almostFullColor', MemoryUsage.almostFullColor),
      ('1000', 'almostFullColor', MemoryUsage.almostFullColor),
      ('2000', 'almostFullColor', MemoryUsage.almostFullColor),
    ]) {
      testWidgets('uses $colorName when $used of 1000 is used', (tester) async {
        await tester.pumpWidget(buildWidget(used, '1000'));
        await tester.pumpAndSettle();

        final indicator = tester.widget<LinearProgressIndicator>(
          find.byType(LinearProgressIndicator),
        );
        expect(indicator.color, color);
      });
    }
  });

  group('MemoryUsage progress value', () {
    for (final (used, total, expected) in [
      ('512', '1024', 0.5),
      ('800', '1000', 0.8),
      ('2000', '1000', 2.0),
    ]) {
      testWidgets('computes $expected for used=$used total=$total',
          (tester) async {
        await tester.pumpWidget(buildWidget(used, total));
        await tester.pumpAndSettle();

        final indicator = tester.widget<LinearProgressIndicator>(
          find.byType(LinearProgressIndicator),
        );
        expect(indicator.value, expected);
      });
    }

    testWidgets('uses backgroundColor on LinearProgressIndicator',
        (tester) async {
      await tester.pumpWidget(buildWidget('512', '1024'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.backgroundColor, MemoryUsage.backgroundColor);
    });
  });

  group('MemoryUsage edge cases', () {
    for (final (used, total) in [
      ('0', '1024'),
      ('0', '0'),
      ('512', '0'),
      ('abc', '1024'),
      ('foo', 'bar'),
      ('1024', 'bar'),
      ('1.5', '1024'),
    ]) {
      testWidgets('shows an empty bar and a dash for used=$used total=$total',
          (tester) async {
        await tester.pumpWidget(buildWidget(used, total));
        await tester.pumpAndSettle();

        final indicator = tester.widget<LinearProgressIndicator>(
          find.byType(LinearProgressIndicator),
        );
        expect(indicator.value, 0.0);
        expect(find.text('-'), findsOneWidget);
      });
    }
  });

  group('MemoryUsage label formatting via widget', () {
    const oneKib = 1024;
    const oneMib = 1024 * oneKib;
    const oneGib = 1024 * oneMib;

    for (final (used, total, label) in [
      (512, oneKib, '512B / 1.0KiB'),
      (oneKib, oneKib, '1.0KiB / 1.0KiB'),
      (oneMib, oneMib, '1.0MiB / 1.0MiB'),
      (oneGib, oneGib, '1.0GiB / 1.0GiB'),
      (oneGib, 2 * oneGib, '1.0GiB / 2.0GiB'),
      (oneMib, oneGib, '1.0MiB / 1.0GiB'),
    ]) {
      testWidgets('shows "$label"', (tester) async {
        await tester.pumpWidget(buildWidget('$used', '$total'));
        await tester.pumpAndSettle();

        expect(find.text(label), findsOneWidget);
      });
    }
  });
}
