import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_table/search_box.dart';

void main() {
  group('SearchBox', () {
    Widget buildApp({required Widget child}) {
      return ProviderScope(
        child: MaterialApp(
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          supportedLocales: AppLocalizations.supportedLocales,
          home: Scaffold(body: child),
        ),
      );
    }

    testWidgets('renders a TextField', (tester) async {
      await tester.pumpWidget(buildApp(child: const SearchBox()));
      expect(find.byType(TextField), findsOneWidget);
    });

    testWidgets('shows a search suffix icon', (tester) async {
      await tester.pumpWidget(buildApp(child: const SearchBox()));
      expect(find.byIcon(Icons.search), findsOneWidget);
    });

    testWidgets('typing text updates searchNameProvider', (tester) async {
      late WidgetRef capturedRef;

      await tester.pumpWidget(
        buildApp(
          child: Consumer(
            builder: (_, ref, __) {
              capturedRef = ref;
              return const SearchBox();
            },
          ),
        ),
      );

      await tester.enterText(find.byType(TextField), 'myvm');

      expect(capturedRef.read(searchNameProvider), equals('myvm'));
    });

    testWidgets('clearing the text resets searchNameProvider to empty',
        (tester) async {
      late WidgetRef capturedRef;

      await tester.pumpWidget(
        buildApp(
          child: Consumer(
            builder: (_, ref, __) {
              capturedRef = ref;
              return const SearchBox();
            },
          ),
        ),
      );

      await tester.enterText(find.byType(TextField), 'filter');
      expect(capturedRef.read(searchNameProvider), equals('filter'));

      await tester.enterText(find.byType(TextField), '');
      expect(capturedRef.read(searchNameProvider), equals(''));
    });
  });
}
