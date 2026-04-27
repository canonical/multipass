import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/copyable_text.dart';
import 'package:multipass_gui/display_field.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

Widget buildApp(Widget child) => MaterialApp(
      locale: const Locale('en'),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(body: child),
    );

void main() {
  group('CopyableText', () {
    testWidgets('renders plain Text when text is "-"', (tester) async {
      await tester.pumpWidget(buildApp(const CopyableText('-')));
      await tester.pumpAndSettle();

      expect(find.text('-'), findsOneWidget);
      expect(find.byType(GestureDetector), findsNothing);
    });

    testWidgets('renders GestureDetector when text is not "-"', (tester) async {
      await tester.pumpWidget(buildApp(const CopyableText('192.168.1.1')));
      await tester.pumpAndSettle();

      expect(find.byType(GestureDetector), findsOneWidget);
    });

    testWidgets('renders MouseRegion with click cursor when text is not "-"',
        (tester) async {
      await tester.pumpWidget(buildApp(const CopyableText('192.168.1.1')));
      await tester.pumpAndSettle();

      final mouseRegions =
          tester.widgetList<MouseRegion>(find.byType(MouseRegion));
      expect(
        mouseRegions.any((r) => r.cursor == SystemMouseCursors.click),
        isTrue,
      );
    });

    testWidgets('displays the provided text value', (tester) async {
      const value = 'hello-world';
      await tester.pumpWidget(buildApp(const CopyableText(value)));
      await tester.pumpAndSettle();

      expect(find.text(value), findsOneWidget);
    });

    testWidgets('applies the provided TextStyle', (tester) async {
      const style = TextStyle(fontSize: 20);
      await tester.pumpWidget(
        buildApp(const CopyableText('some text', style: style)),
      );
      await tester.pumpAndSettle();

      final text = tester.widget<Text>(find.text('some text'));
      expect(text.style, equals(style));
    });

    testWidgets('GestureDetector remains after tap (state rebuilds correctly)',
        (tester) async {
      await tester.pumpWidget(buildApp(const CopyableText('10.0.0.1')));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(GestureDetector));
      await tester.pumpAndSettle();

      expect(find.byType(GestureDetector), findsOneWidget);
    });

    testWidgets('no MouseRegion with click cursor when text is "-"',
        (tester) async {
      await tester.pumpWidget(buildApp(const CopyableText('-')));
      await tester.pumpAndSettle();

      final mouseRegions =
          tester.widgetList<MouseRegion>(find.byType(MouseRegion));
      expect(
        mouseRegions.any((r) => r.cursor == SystemMouseCursors.click),
        isFalse,
      );
    });
  });

  group('DisplayField', () {
    testWidgets('renders label Text and value Text when both are provided',
        (tester) async {
      await tester.pumpWidget(
        buildApp(
          const DisplayField(label: 'IP Address', text: '192.168.1.1'),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('IP Address'), findsOneWidget);
      expect(find.text('192.168.1.1'), findsOneWidget);
    });

    testWidgets('renders no label Text when label is null', (tester) async {
      await tester.pumpWidget(
        buildApp(const DisplayField(text: '192.168.1.1')),
      );
      await tester.pumpAndSettle();

      expect(find.text('192.168.1.1'), findsOneWidget);
      // Only the value text should be present; no separate label widget.
      expect(find.byType(Text), findsOneWidget);
    });

    testWidgets('renders no value widget when text is null', (tester) async {
      await tester.pumpWidget(
        buildApp(const DisplayField(label: 'CPU')),
      );
      await tester.pumpAndSettle();

      expect(find.text('CPU'), findsOneWidget);
      // Only the label text; no SizedBox/value widget.
      expect(find.byType(SizedBox), findsNothing);
    });

    testWidgets('renders an empty Row when both label and text are null',
        (tester) async {
      await tester.pumpWidget(buildApp(const DisplayField()));
      await tester.pumpAndSettle();

      final row = tester.widget<Row>(find.byType(Row));
      expect(row.children, isEmpty);
    });

    testWidgets(
        'renders plain Text (not CopyableText) for value when copyable is false',
        (tester) async {
      await tester.pumpWidget(
        buildApp(
          const DisplayField(label: 'Name', text: 'primary'),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.byType(CopyableText), findsNothing);
      expect(find.text('primary'), findsOneWidget);
    });

    testWidgets('renders CopyableText for value when copyable is true',
        (tester) async {
      await tester.pumpWidget(
        buildApp(
          const DisplayField(
            label: 'IP Address',
            text: '10.0.0.1',
            copyable: true,
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.byType(CopyableText), findsOneWidget);
    });

    testWidgets('label is rendered with the provided labelStyle',
        (tester) async {
      const style = TextStyle(fontSize: 24, color: Colors.red);
      await tester.pumpWidget(
        buildApp(
          const DisplayField(
            label: 'Memory',
            text: '4GB',
            labelStyle: style,
          ),
        ),
      );
      await tester.pumpAndSettle();

      final labelText = tester.widget<Text>(find.text('Memory'));
      expect(labelText.style, equals(style));
    });

    testWidgets('value SizedBox uses the provided width', (tester) async {
      await tester.pumpWidget(
        buildApp(
          const DisplayField(label: 'Disk', text: '50GB', width: 200),
        ),
      );
      await tester.pumpAndSettle();

      final sized = tester.widget<SizedBox>(find.byType(SizedBox));
      expect(sized.width, equals(200));
    });
  });
}
