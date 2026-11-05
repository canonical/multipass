import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/copyable_text.dart';
import 'package:multipass_gui/display_field.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

Widget buildWidget(DisplayField field) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: field),
  );
}

final _fieldTexts = find.descendant(
  of: find.byType(DisplayField),
  matching: find.byType(Text),
);

void main() {
  group('DisplayField', () {
    testWidgets('shows the label and the value', (tester) async {
      await tester.pumpWidget(
        buildWidget(const DisplayField(label: 'Version', text: '1.16.0')),
      );

      expect(find.text('Version'), findsOneWidget);
      expect(find.text('1.16.0'), findsOneWidget);
      expect(_fieldTexts, findsNWidgets(2));
    });

    testWidgets('shows only the value when label is null', (tester) async {
      await tester.pumpWidget(buildWidget(const DisplayField(text: '1.16.0')));

      expect(find.text('1.16.0'), findsOneWidget);
      expect(_fieldTexts, findsOneWidget);
    });

    testWidgets('shows only the label when text is null', (tester) async {
      await tester.pumpWidget(
        buildWidget(const DisplayField(label: 'Copyright')),
      );

      expect(find.text('Copyright'), findsOneWidget);
      expect(_fieldTexts, findsOneWidget);
    });

    testWidgets('shows nothing when label and text are null', (tester) async {
      await tester.pumpWidget(buildWidget(const DisplayField()));

      expect(_fieldTexts, findsNothing);
    });

    testWidgets('value is plain text by default', (tester) async {
      await tester.pumpWidget(
        buildWidget(const DisplayField(label: 'Version', text: '1.16.0')),
      );

      expect(find.byType(CopyableText), findsNothing);
    });

    testWidgets('value is copyable when copyable is true', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          const DisplayField(label: 'Version', text: '1.16.0', copyable: true),
        ),
      );

      expect(find.widgetWithText(CopyableText, '1.16.0'), findsOneWidget);
    });

    testWidgets('applies labelStyle to the label', (tester) async {
      const style = TextStyle(fontSize: 24, color: Colors.red);
      await tester.pumpWidget(
        buildWidget(
          const DisplayField(
              label: 'Version', text: '1.16.0', labelStyle: style),
        ),
      );

      expect(tester.widget<Text>(find.text('Version')).style, style);
    });

    testWidgets('lays out the value with the given width', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          const DisplayField(label: 'Version', text: '1.16.0', width: 200),
        ),
      );

      final valueBox = find
          .ancestor(of: find.text('1.16.0'), matching: find.byType(SizedBox))
          .first;
      expect(tester.getSize(valueBox).width, 200);
    });
  });
}
