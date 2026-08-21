import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/vm_details.dart';

Widget buildWidget({
  required ActiveEditPage? active,
  required List<ActiveEditPage> letEnabledFor,
  required VoidCallback onPressed,
}) {
  return MaterialApp(
    home: Scaffold(
      body: DisableSection(
        active: active,
        letEnabledFor: letEnabledFor,
        child: ElevatedButton(
          onPressed: onPressed,
          child: const Text('content'),
        ),
      ),
    ),
  );
}

double sectionOpacity(WidgetTester tester) {
  final opacity = tester.widget<Opacity>(
    find
        .descendant(
          of: find.byType(DisableSection),
          matching: find.byType(Opacity),
        )
        .first,
  );
  return opacity.opacity;
}

void main() {
  group('DisableSection', () {
    testWidgets('forwards taps to its child when active is null',
        (tester) async {
      var tapped = false;
      await tester.pumpWidget(buildWidget(
        active: null,
        letEnabledFor: const [],
        onPressed: () => tapped = true,
      ));

      await tester.tap(find.text('content'));
      await tester.pumpAndSettle();

      expect(tapped, isTrue);
    });

    testWidgets('forwards taps when the active page is in letEnabledFor',
        (tester) async {
      var tapped = false;
      await tester.pumpWidget(buildWidget(
        active: ActiveEditPage.resources,
        letEnabledFor: const [ActiveEditPage.resources],
        onPressed: () => tapped = true,
      ));

      await tester.tap(find.text('content'));
      await tester.pumpAndSettle();

      expect(tapped, isTrue);
    });

    testWidgets('blocks taps when the active page is not in letEnabledFor',
        (tester) async {
      var tapped = false;
      await tester.pumpWidget(buildWidget(
        active: ActiveEditPage.bridge,
        letEnabledFor: const [ActiveEditPage.resources],
        onPressed: () => tapped = true,
      ));

      await tester.tap(find.text('content'), warnIfMissed: false);
      await tester.pumpAndSettle();

      expect(tapped, isFalse);
    });

    testWidgets('blocks taps when a page is active and letEnabledFor is empty',
        (tester) async {
      var tapped = false;
      await tester.pumpWidget(buildWidget(
        active: ActiveEditPage.mounts,
        letEnabledFor: const [],
        onPressed: () => tapped = true,
      ));

      await tester.tap(find.text('content'), warnIfMissed: false);
      await tester.pumpAndSettle();

      expect(tapped, isFalse);
    });

    testWidgets('renders its child', (tester) async {
      await tester.pumpWidget(buildWidget(
        active: null,
        letEnabledFor: const [],
        onPressed: () {},
      ));

      expect(find.text('content'), findsOneWidget);
    });

    testWidgets('dims a disabled section relative to an enabled one',
        (tester) async {
      await tester.pumpWidget(buildWidget(
        active: null,
        letEnabledFor: const [],
        onPressed: () {},
      ));
      final enabledOpacity = sectionOpacity(tester);

      await tester.pumpWidget(buildWidget(
        active: ActiveEditPage.bridge,
        letEnabledFor: const [ActiveEditPage.resources],
        onPressed: () {},
      ));
      final disabledOpacity = sectionOpacity(tester);

      expect(disabledOpacity, lessThan(enabledOpacity));
    });
  });
}
