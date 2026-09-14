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
    testWidgets('uses normalColor when usage is below 80%', (tester) async {
      await tester.pumpWidget(buildWidget('512', '1024'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.color, MemoryUsage.normalColor);
    });

    testWidgets('uses almostFullColor when usage is exactly 80%',
        (tester) async {
      await tester.pumpWidget(buildWidget('800', '1000'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.color, MemoryUsage.almostFullColor);
    });

    testWidgets('uses almostFullColor when usage is 100%', (tester) async {
      await tester.pumpWidget(buildWidget('1000', '1000'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.color, MemoryUsage.almostFullColor);
    });
  });

  group('MemoryUsage progress value', () {
    testWidgets('computes 0.5 for used=512 total=1024', (tester) async {
      await tester.pumpWidget(buildWidget('512', '1024'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 0.5);
    });

    testWidgets('computes 0.8 for used=800 total=1000', (tester) async {
      await tester.pumpWidget(buildWidget('800', '1000'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 0.8);
    });

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
    testWidgets('shows dash label when used is "0"', (tester) async {
      await tester.pumpWidget(buildWidget('0', '1024'));
      await tester.pumpAndSettle();

      expect(find.text('-'), findsOneWidget);
    });

    testWidgets('value is 0.0 and shows dash when total is "0"',
        (tester) async {
      await tester.pumpWidget(buildWidget('0', '0'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 0.0);
      expect(find.text('-'), findsOneWidget);
    });

    testWidgets('falls back gracefully when used is non-parseable',
        (tester) async {
      await tester.pumpWidget(buildWidget('abc', '1024'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 0.0);
      expect(find.text('-'), findsOneWidget);
    });

    testWidgets('falls back gracefully when both are non-parseable',
        (tester) async {
      await tester.pumpWidget(buildWidget('foo', 'bar'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 0.0);
      expect(find.text('-'), findsOneWidget);
    });

    testWidgets('shows dash when total is "0" but used is non-zero',
        (tester) async {
      // Division by zero yields Infinity; isFinite guard sets value to 0.0.
      await tester.pumpWidget(buildWidget('512', '0'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 0.0);
      expect(find.text('-'), findsOneWidget);
    });

    testWidgets('clamps to full when used exceeds total', (tester) async {
      // value > 1.0 is clamped by LinearProgressIndicator; almostFullColor applied.
      await tester.pumpWidget(buildWidget('2000', '1000'));
      await tester.pumpAndSettle();

      final indicator = tester.widget<LinearProgressIndicator>(
        find.byType(LinearProgressIndicator),
      );
      expect(indicator.value, 2.0);
      expect(indicator.color, MemoryUsage.almostFullColor);
    });
  });

  group('MemoryUsage label formatting via widget', () {
    testWidgets('formats 1 GiB correctly', (tester) async {
      const oneGib = 1073741824;
      await tester.pumpWidget(buildWidget('$oneGib', '$oneGib'));
      await tester.pumpAndSettle();

      expect(find.textContaining('1.0GiB'), findsWidgets);
    });

    testWidgets('formats 1 MiB correctly', (tester) async {
      const oneMib = 1048576;
      await tester.pumpWidget(buildWidget('$oneMib', '$oneMib'));
      await tester.pumpAndSettle();

      expect(find.textContaining('1.0MiB'), findsWidgets);
    });

    testWidgets('formats 1 KiB correctly', (tester) async {
      const oneKib = 1024;
      await tester.pumpWidget(buildWidget('$oneKib', '$oneKib'));
      await tester.pumpAndSettle();

      expect(find.textContaining('1.0KiB'), findsWidgets);
    });

    testWidgets('formats bytes below 1 KiB correctly', (tester) async {
      await tester.pumpWidget(buildWidget('512', '1024'));
      await tester.pumpAndSettle();

      expect(find.textContaining('512B'), findsOneWidget);
    });

    testWidgets('label shows used / total format', (tester) async {
      const oneGib = 1073741824;
      const twoGib = 2 * oneGib;
      await tester.pumpWidget(buildWidget('$oneGib', '$twoGib'));
      await tester.pumpAndSettle();

      expect(find.text('1.0GiB / 2.0GiB'), findsOneWidget);
    });

    testWidgets('label uses mixed formats', (tester) async {
      const oneGib = 1073741824;
      const oneMib = 1048576;
      await tester.pumpWidget(buildWidget('$oneMib', '$oneGib'));
      await tester.pumpAndSettle();

      expect(find.text('1.0MiB / 1.0GiB'), findsOneWidget);
    });
  });
}
