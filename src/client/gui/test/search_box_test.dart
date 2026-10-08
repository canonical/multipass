import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_table/search_box.dart';

void main() {
  group('SearchBox', () {
    Future<ProviderContainer> pumpSearchBox(WidgetTester tester) async {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: container,
          child: MaterialApp(
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: const Scaffold(body: SearchBox()),
          ),
        ),
      );
      return container;
    }

    testWidgets('typing text updates searchNameProvider', (tester) async {
      final container = await pumpSearchBox(tester);

      await tester.enterText(find.byType(TextField), 'myvm');

      expect(container.read(searchNameProvider), 'myvm');
    });

    testWidgets('clearing the text resets searchNameProvider to empty',
        (tester) async {
      final container = await pumpSearchBox(tester);

      await tester.enterText(find.byType(TextField), 'filter');
      expect(container.read(searchNameProvider), 'filter');

      await tester.enterText(find.byType(TextField), '');
      expect(container.read(searchNameProvider), isEmpty);
    });
  });
}
