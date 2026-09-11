import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/vm_details.dart';

Widget _buildApp(ActiveEditPage? active, List<ActiveEditPage> letEnabledFor) {
  return MaterialApp(
    home: Scaffold(
      body: DisableSection(
        active: active,
        letEnabledFor: letEnabledFor,
        child: const SizedBox(key: Key('child'), width: 100, height: 100),
      ),
    ),
  );
}

void main() {
  const fullyVisible = 1.0;
  const dimmed = 0.5;

  group('DisableSection', () {
    Opacity findOpacity(WidgetTester tester) {
      return tester
          .widgetList<Opacity>(
            find.ancestor(
              of: find.byKey(const Key('child')),
              matching: find.byType(Opacity),
            ),
          )
          .first;
    }

    IgnorePointer findIgnorePointer(WidgetTester tester) {
      return tester
          .widgetList<IgnorePointer>(
            find.ancestor(
              of: find.byKey(const Key('child')),
              matching: find.byType(IgnorePointer),
            ),
          )
          .first;
    }

    testWidgets('child is fully visible when active is null', (tester) async {
      await tester.pumpWidget(_buildApp(null, []));
      await tester.pump();

      expect(findOpacity(tester).opacity, equals(fullyVisible));
      expect(findIgnorePointer(tester).ignoring, isFalse);
    });

    testWidgets('child is dimmed and non-interactive when not in letEnabledFor',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(ActiveEditPage.resources, [ActiveEditPage.bridge]),
      );
      await tester.pump();

      expect(findOpacity(tester).opacity, equals(dimmed));
      expect(findIgnorePointer(tester).ignoring, isTrue);
    });

    testWidgets('child is fully visible when page is in letEnabledFor',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(ActiveEditPage.resources, [ActiveEditPage.resources]),
      );
      await tester.pump();

      expect(findOpacity(tester).opacity, equals(fullyVisible));
      expect(findIgnorePointer(tester).ignoring, isFalse);
    });

    testWidgets(
        'child is fully visible when active is null regardless of letEnabledFor',
        (tester) async {
      await tester.pumpWidget(_buildApp(null, [ActiveEditPage.mounts]));
      await tester.pump();

      expect(findOpacity(tester).opacity, equals(fullyVisible));
    });

    testWidgets(
        'child is dimmed when active page differs even if letEnabledFor has entries',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(
          ActiveEditPage.bridge,
          [ActiveEditPage.resources, ActiveEditPage.mounts],
        ),
      );
      await tester.pump();

      expect(findOpacity(tester).opacity, equals(dimmed));
    });
  });
}
