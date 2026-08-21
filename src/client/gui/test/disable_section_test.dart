import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/vm_details.dart';

Widget buildWidget({
  required ActiveEditPage? active,
  required List<ActiveEditPage> letEnabledFor,
}) {
  return MaterialApp(
    home: Scaffold(
      body: DisableSection(
        active: active,
        letEnabledFor: letEnabledFor,
        child: const Text('content'),
      ),
    ),
  );
}

void main() {
  group('DisableSection', () {
    testWidgets('is not disabled when active is null', (tester) async {
      await tester.pumpWidget(
        buildWidget(active: null, letEnabledFor: []),
      );
      final section = find.byType(DisableSection);
      final opacity = tester.widget<Opacity>(
        find.descendant(of: section, matching: find.byType(Opacity)).first,
      );
      expect(opacity.opacity, equals(1.0));
      final pointer = tester.widget<IgnorePointer>(
        find
            .descendant(of: section, matching: find.byType(IgnorePointer))
            .first,
      );
      expect(pointer.ignoring, isFalse);
    });

    testWidgets('is not disabled when active is in letEnabledFor',
        (tester) async {
      await tester.pumpWidget(
        buildWidget(
          active: ActiveEditPage.resources,
          letEnabledFor: [ActiveEditPage.resources],
        ),
      );
      final section = find.byType(DisableSection);
      final opacity = tester.widget<Opacity>(
        find.descendant(of: section, matching: find.byType(Opacity)).first,
      );
      expect(opacity.opacity, equals(1.0));
      final pointer = tester.widget<IgnorePointer>(
        find
            .descendant(of: section, matching: find.byType(IgnorePointer))
            .first,
      );
      expect(pointer.ignoring, isFalse);
    });

    testWidgets('is disabled when active is not in letEnabledFor',
        (tester) async {
      await tester.pumpWidget(
        buildWidget(
          active: ActiveEditPage.bridge,
          letEnabledFor: [ActiveEditPage.resources],
        ),
      );
      final section = find.byType(DisableSection);
      final opacity = tester.widget<Opacity>(
        find.descendant(of: section, matching: find.byType(Opacity)).first,
      );
      expect(opacity.opacity, equals(0.5));
      final pointer = tester.widget<IgnorePointer>(
        find
            .descendant(of: section, matching: find.byType(IgnorePointer))
            .first,
      );
      expect(pointer.ignoring, isTrue);
    });

    testWidgets('is disabled when active is set and letEnabledFor is empty',
        (tester) async {
      await tester.pumpWidget(
        buildWidget(
          active: ActiveEditPage.mounts,
          letEnabledFor: [],
        ),
      );
      final section = find.byType(DisableSection);
      final opacity = tester.widget<Opacity>(
        find.descendant(of: section, matching: find.byType(Opacity)).first,
      );
      expect(opacity.opacity, equals(0.5));
    });

    testWidgets('renders child widget', (tester) async {
      await tester.pumpWidget(
        buildWidget(active: null, letEnabledFor: []),
      );
      expect(find.text('content'), findsOneWidget);
    });
  });
}
