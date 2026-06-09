import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/image_card.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';

import '../helpers.dart';

final _l10n = AppLocalizationsEn();

// Shared ImageInfo fixtures
ImageInfo makeImage({
  String os = 'Ubuntu',
  String release = '24.04',
  String codename = 'noble',
  List<String> aliases = const ['24.04', 'lts'],
}) =>
    ImageInfo(os: os, release: release, codename: codename, aliases: aliases);

final noble = makeImage(); // Ubuntu 24.04 noble ['24.04', 'lts']
final jammy =
    makeImage(release: '22.04', codename: 'jammy', aliases: ['22.04']);
final core20 =
    makeImage(release: 'core20', codename: 'core20', aliases: ['core20']);
final core22 = makeImage(release: '22', codename: '', aliases: ['core22']);
final debian = makeImage(
    os: 'Debian', release: '12', codename: 'bookworm', aliases: ['debian']);
final fedora = makeImage(
    os: 'Fedora', release: '39', codename: 'fedora', aliases: ['fedora']);
final alpine =
    makeImage(os: 'Alpine', release: '3.20', codename: '', aliases: ['alpine']);

Widget buildApp(ImageCard card) => withFakeSvgAssetBundle(
      ProviderScope(
        overrides: [
          randomNameProvider.overrideWith((ref) => 'test-vm'),
        ],
        child: MaterialApp(
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          supportedLocales: AppLocalizations.supportedLocales,
          home: Scaffold(
            body: SizedBox(
              width: 500,
              height: 600,
              child: card,
            ),
          ),
        ),
      ),
    );

void main() {
  group('ImageCard', () {
    group('title', () {
      testWidgets('shows "Ubuntu Server" for Ubuntu images', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.imageCardTitleUbuntuServer), findsOneWidget);
      });

      testWidgets('shows "Ubuntu Core" when alias contains "core"',
          (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: core22,
          versions: [core22],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.imageCardTitleUbuntuCore), findsOneWidget);
      });

      testWidgets('shows "Debian" for Debian images', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: debian,
          versions: [debian],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.imageCardTitleDebian), findsOneWidget);
      });

      testWidgets('shows "Fedora" for Fedora images', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: fedora,
          versions: [fedora],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.imageCardTitleFedora), findsOneWidget);
      });

      testWidgets('shows OS name for unrecognised OS', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: alpine,
          versions: [alpine],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Alpine'), findsOneWidget);
      });
    });

    group('version label', () {
      testWidgets('shows "release (codename)" when they differ',
          (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('24.04 (noble)'), findsOneWidget);
      });

      testWidgets('shows release only when release equals codename',
          (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: core20,
          versions: [core20],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('core20'), findsOneWidget);
      });

      testWidgets('shows release only when codename is empty', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: alpine,
          versions: [alpine],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('3.20'), findsOneWidget);
        expect(find.textContaining('()'), findsNothing);
      });
    });

    group('version dropdown', () {
      testWidgets('is disabled when only one version is provided',
          (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        final dropdown = tester.widget<DropdownButton<String>>(
          find.byType(DropdownButton<String>),
        );
        expect(dropdown.onChanged, isNull);
      });

      testWidgets('is enabled when multiple versions are provided',
          (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [jammy, noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        final dropdown = tester.widget<DropdownButton<String>>(
          find.byType(DropdownButton<String>),
        );
        expect(dropdown.onChanged, isNotNull);
      });

      testWidgets('selecting a version updates the displayed label',
          (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [jammy, noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        await tester.tap(find.byType(DropdownButton<String>));
        await tester.pumpAndSettle();

        await tester.tap(find.text('22.04 (jammy)').last);
        await tester.pumpAndSettle();

        expect(find.text('22.04 (jammy)'), findsOneWidget);
      });
    });

    group('buttons', () {
      testWidgets('Launch button is present', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.commonLaunch), findsOneWidget);
      });

      testWidgets('Configure button is present', (tester) async {
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: noble,
          versions: [noble],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text(_l10n.commonConfigure), findsOneWidget);
      });
    });
  });
}
