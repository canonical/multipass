import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/image_card.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';

import 'helpers.dart';

ImageInfo makeImage({
  String os = 'Ubuntu',
  String release = '24.04',
  String codename = 'noble',
  List<String> aliases = const ['24.04', 'lts'],
}) =>
    ImageInfo(os: os, release: release, codename: codename, aliases: aliases);

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
        final image = makeImage(os: 'Ubuntu');
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Ubuntu Server'), findsOneWidget);
      });

      testWidgets('shows "Ubuntu Core" when alias contains "core"',
          (tester) async {
        final image = makeImage(
          os: 'Ubuntu',
          release: '22',
          codename: '',
          aliases: ['core22'],
        );
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Ubuntu Core'), findsOneWidget);
      });

      testWidgets('shows "Debian" for Debian images', (tester) async {
        final image = makeImage(
          os: 'Debian',
          release: '12',
          codename: 'bookworm',
          aliases: ['debian'],
        );
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Debian'), findsOneWidget);
      });

      testWidgets('shows "Fedora" for Fedora images', (tester) async {
        final image = makeImage(
          os: 'Fedora',
          release: '39',
          codename: 'fedora',
          aliases: ['fedora'],
        );
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Fedora'), findsOneWidget);
      });

      testWidgets('shows OS name for unrecognised OS', (tester) async {
        final image = makeImage(
          os: 'Alpine',
          release: '3.20',
          codename: '',
          aliases: ['alpine'],
        );
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
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
        final image = makeImage(release: '24.04', codename: 'noble');
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('24.04 (noble)'), findsOneWidget);
      });

      testWidgets('shows release only when release equals codename',
          (tester) async {
        final image = makeImage(
          release: 'core20',
          codename: 'core20',
          aliases: ['core20'],
        );
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('core20'), findsOneWidget);
      });
    });

    group('version dropdown', () {
      testWidgets('is disabled when only one version is provided',
          (tester) async {
        final image = makeImage();
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
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
        final v1 =
            makeImage(release: '22.04', codename: 'jammy', aliases: ['22.04']);
        final v2 = makeImage(
            release: '24.04', codename: 'noble', aliases: ['24.04', 'lts']);
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: v2,
          versions: [v1, v2],
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
        final v1 =
            makeImage(release: '22.04', codename: 'jammy', aliases: ['22.04']);
        final v2 = makeImage(
            release: '24.04', codename: 'noble', aliases: ['24.04', 'lts']);
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: v2,
          versions: [v1, v2],
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
        final image = makeImage();
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Launch'), findsOneWidget);
      });

      testWidgets('Configure button is present', (tester) async {
        final image = makeImage();
        await tester.pumpWidget(buildApp(ImageCard(
          parentImage: image,
          versions: [image],
          width: 500,
          imageKey: 'k',
        )));
        await tester.pumpAndSettle();

        expect(find.text('Configure'), findsOneWidget);
      });
    });
  });
}
