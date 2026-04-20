import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/catalogue/catalogue.dart';
import 'package:multipass_gui/catalogue/image_card.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';

final _l10n = AppLocalizationsEn();

// Mirrors what the daemon reports for each remote.
final _noble = ImageInfo(
  os: 'Ubuntu',
  release: '24.04 LTS',
  codename: 'Noble Numbat',
  aliases: ['24.04', 'noble', 'lts'],
  remoteName: 'release',
);
final _jammy = ImageInfo(
  os: 'Ubuntu',
  release: '22.04 LTS',
  codename: 'Jammy Jellyfish',
  aliases: ['22.04', 'jammy'],
  remoteName: 'release',
);
final _questing = ImageInfo(
  os: 'Ubuntu',
  release: '25.10',
  codename: 'Questing Quokka',
  aliases: ['25.10', 'questing'],
  remoteName: 'release',
);
final _dailyNoble = ImageInfo(
  os: 'Ubuntu',
  release: '24.04 LTS',
  codename: 'Noble Numbat',
  aliases: ['24.04', 'noble', 'lts'],
  remoteName: 'daily',
);
final _devel = ImageInfo(
  os: 'Ubuntu',
  release: '26.04',
  codename: 'Resolute Raccoon',
  aliases: ['26.04', 'resolute', 'devel'],
  remoteName: 'daily',
);
final _core24 = ImageInfo(
  os: 'Ubuntu',
  release: 'Core 24',
  codename: 'Core 24',
  aliases: ['core24'],
  remoteName: 'core',
);
final _core22 = ImageInfo(
  os: 'Ubuntu',
  release: 'Core 22',
  codename: 'Core 22',
  aliases: ['core22'],
  remoteName: 'core',
);
final _snapcraft = ImageInfo(
  os: 'Ubuntu',
  release: 'Snapcraft builder for Core 24',
  codename: 'Snapcraft builder for Core 24',
  aliases: ['core24'],
  remoteName: 'snapcraft',
);
final _debian = ImageInfo(
  os: 'Debian',
  release: '13',
  codename: 'Trixie',
  aliases: ['debian', 'trixie'],
);
final _fedora = ImageInfo(
  os: 'Fedora',
  release: '44',
  codename: '44',
  aliases: ['fedora'],
);

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
      notifier('key').set(_jammy);
      notifier('key').set(_noble);
      expect(state('key'), same(_noble));
    });

    test('each image key keeps its own selection', () {
      notifier('ubuntu').set(_noble);
      expect(state('ubuntu'), same(_noble));
      expect(state('core'), isNull);
    });
  });

  group('CatalogueScreen', () {
    testWidgets('shows the Ubuntu card, then the Core card, then the others',
        (tester) async {
      final cards =
          await _pumpCards(tester, [_debian, _core24, _fedora, _noble]);

      expect(
        cards.map((c) => c.parentImage),
        orderedEquals([_noble, _core24, _fedora, _debian]),
      );
    });

    testWidgets('lists Ubuntu releases newest first with the LTS as parent',
        (tester) async {
      final cards = await _pumpCards(tester, [_jammy, _noble, _questing]);

      expect(cards, hasLength(1));
      expect(cards.single.parentImage, same(_noble));
      expect(cards.single.versions, orderedEquals([_questing, _noble, _jammy]));
    });

    testWidgets('uses the newest Ubuntu release as parent without an LTS',
        (tester) async {
      final cards = await _pumpCards(tester, [_jammy, _questing]);

      expect(cards.single.parentImage, same(_questing));
    });

    testWidgets('adds daily images only for releases not in the release remote',
        (tester) async {
      final cards = await _pumpCards(tester, [_dailyNoble, _noble, _devel]);

      expect(cards.single.versions, orderedEquals([_devel, _noble]));
    });

    testWidgets('lists Core images newest first in their own card',
        (tester) async {
      final cards = await _pumpCards(tester, [_core22, _core24]);

      expect(cards, hasLength(1));
      expect(cards.single.parentImage, same(_core24));
      expect(cards.single.versions, orderedEquals([_core24, _core22]));
    });

    testWidgets('shows each image without a remote in a separate card',
        (tester) async {
      final cards = await _pumpCards(tester, [_debian, _fedora]);

      expect(cards.map((c) => c.versions), [
        [_fedora],
        [_debian],
      ]);
    });

    testWidgets('leaves out snapcraft images', (tester) async {
      final cards = await _pumpCards(tester, [_snapcraft, _noble]);

      expect(cards.single.versions, orderedEquals([_noble]));
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
        return [_noble];
      }));
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.catalogueRefresh));
      await tester.pumpAndSettle();

      expect(calls, 2);
      expect(find.byType(ImageCard), findsOneWidget);
    });
  });
}
