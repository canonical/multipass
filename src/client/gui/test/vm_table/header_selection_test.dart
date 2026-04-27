import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_table/header_selection.dart';
import 'package:multipass_gui/vm_table/vm_table_headers.dart';

import '../helpers.dart';

void main() {
  group('HeaderSelectionTile', () {
    Widget buildApp(String name) {
      return withFakeSvgAssetBundle(
        ProviderScope(
          child: MaterialApp(
            locale: const Locale('en'),
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(body: HeaderSelectionTile(name, name)),
          ),
        ),
      );
    }

    testWidgets('shows checkbox checked when header is enabled',
        (tester) async {
      await tester.pumpWidget(buildApp('STATE'));
      await tester.pump();

      final checkbox = tester.widget<CheckboxListTile>(
        find.byType(CheckboxListTile),
      );
      expect(checkbox.value, isTrue);
    });

    testWidgets('tapping checkbox disables the header', (tester) async {
      late WidgetRef capturedRef;

      await tester.pumpWidget(
        ProviderScope(
          child: MaterialApp(
            locale: const Locale('en'),
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(
              body: Consumer(
                builder: (_, ref, __) {
                  capturedRef = ref;
                  return HeaderSelectionTile('STATE', 'State');
                },
              ),
            ),
          ),
        ),
      );
      await tester.pump();

      await tester.tap(find.byType(CheckboxListTile));
      await tester.pump();

      expect(capturedRef.read(enabledHeadersProvider)['STATE'], isFalse);
    });

    testWidgets('tapping again re-enables the header', (tester) async {
      late WidgetRef capturedRef;

      await tester.pumpWidget(
        ProviderScope(
          child: MaterialApp(
            locale: const Locale('en'),
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(
              body: Consumer(
                builder: (_, ref, __) {
                  capturedRef = ref;
                  return HeaderSelectionTile('STATE', 'State');
                },
              ),
            ),
          ),
        ),
      );
      await tester.pump();

      await tester.tap(find.byType(CheckboxListTile));
      await tester.pump();
      await tester.tap(find.byType(CheckboxListTile));
      await tester.pump();

      expect(capturedRef.read(enabledHeadersProvider)['STATE'], isTrue);
    });

    testWidgets('shows the provided label text', (tester) async {
      await tester.pumpWidget(
        ProviderScope(
          child: MaterialApp(
            home: Scaffold(
              body: HeaderSelectionTile('CPU USAGE', 'CPU Usage'),
            ),
          ),
        ),
      );

      expect(find.text('CPU Usage'), findsOneWidget);
    });
  });

  group('HeaderSelection', () {
    Widget buildApp() {
      return withFakeSvgAssetBundle(
        ProviderScope(
          child: MaterialApp(
            locale: const Locale('en'),
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: const Scaffold(body: Center(child: HeaderSelection())),
          ),
        ),
      );
    }

    testWidgets('renders the Columns button text', (tester) async {
      await tester.pumpWidget(buildApp());
      await tester.pumpAndSettle();

      expect(find.text('Columns'), findsOneWidget);
    });

    testWidgets('renders a PopupMenuButton', (tester) async {
      await tester.pumpWidget(buildApp());
      await tester.pumpAndSettle();

      expect(
        find.byWidgetPredicate((w) => w is PopupMenuButton),
        findsOneWidget,
      );
    });

    testWidgets('opening the popup shows a checkbox for each column header',
        (tester) async {
      await tester.pumpWidget(buildApp());
      await tester.pumpAndSettle();

      await tester.tap(find.byWidgetPredicate((w) => w is PopupMenuButton));
      await tester.pumpAndSettle();

      expect(
        find.byType(CheckboxListTile),
        findsNWidgets(headers.skip(2).length),
      );
    });
  });
}
