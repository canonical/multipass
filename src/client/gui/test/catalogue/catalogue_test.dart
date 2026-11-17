import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/catalogue/catalogue.dart';
import 'package:multipass_gui/catalogue/image_card.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';

import 'image_fixtures.dart';

final _l10n = AppLocalizationsEn();

Widget _buildApp(Future<List<ImageInfo>> Function() fetchImages) {
  return ProviderScope(
    // Riverpod retries failed providers by default, which would refetch the
    // images without the Refresh button being pressed.
    retry: (_, __) => null,
    overrides: [imagesProvider.overrideWith((ref) => fetchImages())],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const CatalogueScreen(),
    ),
  );
}

Future<List<ImageCard>> _pumpCards(
  WidgetTester tester,
  List<ImageInfo> images,
) async {
  await tester.pumpWidget(_buildApp(() async => images));
  await tester.pumpAndSettle();
  return tester.widgetList<ImageCard>(find.byType(ImageCard)).toList();
}

void main() {
  group('SelectedImageNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    ImageInfo? state(String key) => container.read(selectedImageProvider(key));
    SelectedImageNotifier notifier(String key) =>
        container.read(selectedImageProvider(key).notifier);

    test('initial state is null', () {
      expect(state('key'), isNull);
    });

    test('set() replaces the selected image', () {
      notifier('key').set(jammy);
      notifier('key').set(noble);
      expect(state('key'), same(noble));
    });

    test('each image key keeps its own selection', () {
      notifier('ubuntu').set(noble);
      expect(state('ubuntu'), same(noble));
      expect(state('core'), isNull);
    });
  });

  group('CatalogueScreen', () {
    testWidgets('shows the Ubuntu card, then the Core card, then the others',
        (tester) async {
      final cards = await _pumpCards(tester, [debian, core24, fedora, noble]);

      expect(
        cards.map((c) => c.parentImage),
        orderedEquals([noble, core24, fedora, debian]),
      );
    });

    testWidgets('lists Ubuntu releases newest first with the LTS as parent',
        (tester) async {
      final cards = await _pumpCards(tester, [jammy, noble, questing]);

      expect(cards, hasLength(1));
      expect(cards.single.parentImage, same(noble));
      expect(cards.single.versions, orderedEquals([questing, noble, jammy]));
    });

    testWidgets('uses the newest Ubuntu release as parent without an LTS',
        (tester) async {
      final cards = await _pumpCards(tester, [jammy, questing]);

      expect(cards.single.parentImage, same(questing));
    });

    testWidgets('adds daily images only for releases not in the release remote',
        (tester) async {
      final cards = await _pumpCards(tester, [dailyNoble, noble, devel]);

      expect(cards.single.versions, orderedEquals([devel, noble]));
    });

    testWidgets('lists Core images newest first in their own card',
        (tester) async {
      final cards = await _pumpCards(tester, [core22, core24]);

      expect(cards, hasLength(1));
      expect(cards.single.parentImage, same(core24));
      expect(cards.single.versions, orderedEquals([core24, core22]));
    });

    testWidgets('shows each image without a remote in a separate card',
        (tester) async {
      final cards = await _pumpCards(tester, [debian, fedora]);

      expect(cards.map((c) => c.versions), [
        [fedora],
        [debian],
      ]);
    });

    testWidgets('leaves out snapcraft images', (tester) async {
      final cards = await _pumpCards(tester, [snapcraft, noble]);

      expect(cards.single.versions, orderedEquals([noble]));
    });

    testWidgets('shows only the message of a GrpcError', (tester) async {
      await tester.pumpWidget(
        _buildApp(() => Future.error(GrpcError.unavailable('daemon is down'))),
      );
      await tester.pumpAndSettle();

      expect(find.text(_l10n.catalogueLoadError('daemon is down')),
          findsOneWidget);
    });

    testWidgets('refetches the images when Refresh is pressed', (tester) async {
      var calls = 0;
      await tester.pumpWidget(_buildApp(() async {
        calls++;
        if (calls == 1) throw Exception('boom');
        return [noble];
      }));
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.catalogueRefresh));
      await tester.pumpAndSettle();

      expect(calls, 2);
      expect(find.byType(ImageCard), findsOneWidget);
    });
  });
}
