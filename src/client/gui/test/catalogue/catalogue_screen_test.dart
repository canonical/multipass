import 'dart:async';

import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/catalogue.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

import '../helpers.dart';

Widget _scope(Widget child) {
  return withFakeSvgAssetBundle(
    MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: child,
    ),
  );
}

void main() {
  group('CatalogueScreen', () {
    testWidgets('shows a loading indicator while images are fetching',
        (tester) async {
      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            imagesProvider.overrideWith(
              (ref) => Completer<List<ImageInfo>>().future,
            ),
          ],
          child: _scope(const CatalogueScreen()),
        ),
      );
      // One pump to trigger the first build before the future resolves.
      await tester.pump();

      expect(find.byType(CircularProgressIndicator), findsOneWidget);
    });

    testWidgets('shows an error message when images fail to load',
        (tester) async {
      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            imagesProvider.overrideWith((ref) async {
              throw Exception('Connection failed');
            }),
          ],
          child: _scope(const CatalogueScreen()),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.textContaining('Connection failed'), findsOneWidget);
    });

    testWidgets('shows a refresh button when images fail to load',
        (tester) async {
      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            imagesProvider.overrideWith((ref) async {
              throw Exception('Load error');
            }),
          ],
          child: _scope(const CatalogueScreen()),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Refresh'), findsOneWidget);
    });

    testWidgets('shows the welcome title when images are successfully loaded',
        (tester) async {
      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            imagesProvider.overrideWith((ref) async => []),
          ],
          child: _scope(const CatalogueScreen()),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Welcome to Multipass'), findsOneWidget);
    });
  });
}
