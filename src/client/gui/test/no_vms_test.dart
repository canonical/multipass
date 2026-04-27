import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_table/no_vms.dart';

import 'helpers.dart';

Widget buildWidget() {
  return withFakeSvgAssetBundle(
    ProviderScope(
      overrides: [
        vmNamesProvider.overrideWith((ref) => BuiltSet<String>()),
      ],
      child: const MaterialApp(
        locale: const Locale('en'),
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(body: NoVms()),
      ),
    ),
  );
}

void main() {
  group('NoVms', () {
    testWidgets('renders the title text', (tester) async {
      await tester.pumpWidget(buildWidget());
      await tester.pump();

      expect(find.byType(Text), findsWidgets);
    });

    testWidgets('renders rich text containing the catalogue link',
        (tester) async {
      await tester.pumpWidget(buildWidget());
      await tester.pump();

      expect(find.byType(RichText), findsWidgets);
    });
  });
}
