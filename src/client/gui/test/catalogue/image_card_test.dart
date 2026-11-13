import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:fpdart/fpdart.dart';
import 'package:multipass_gui/catalogue/image_card.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';

const _vmName = 'test-vm';

final _l10n = AppLocalizationsEn();

// Fixtures mirror the image info the daemon reports for each remote.
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
final _core24 = ImageInfo(
  os: 'Ubuntu',
  release: 'Core 24',
  codename: 'Core 24',
  aliases: ['core24'],
  remoteName: 'core',
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
final _alpine = ImageInfo(os: 'Alpine', release: '3.20', aliases: ['alpine']);

class _FakeGrpcClient extends Fake implements GrpcClient {
  final launchRequests = <LaunchRequest>[];

  @override
  Stream<Either<LaunchReply, MountReply>?> launch(
    LaunchRequest request, {
    List<MountRequest> mountRequests = const [],
    Future<void>? cancel,
  }) {
    launchRequests.add(request);
    return const Stream.empty();
  }
}

Future<ProviderContainer> _pumpCard(
  WidgetTester tester, {
  required ImageInfo parentImage,
  List<ImageInfo>? versions,
  GrpcClient? grpcClient,
}) async {
  final container = ProviderContainer(
    overrides: [
      randomNameProvider.overrideWith((_) => _vmName),
      azSupportedProvider.overrideWithValue(false),
      if (grpcClient != null) grpcClientProvider.overrideWithValue(grpcClient),
    ],
  );
  addTearDown(container.dispose);

  await tester.pumpWidget(
    UncontrolledProviderScope(
      container: container,
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          endDrawer: const Drawer(),
          body: SizedBox(
            width: 500,
            height: 600,
            child: ImageCard(
              parentImage: parentImage,
              versions: versions ?? [parentImage],
              width: 500,
              imageKey: 'key',
            ),
          ),
        ),
      ),
    ),
  );
  await tester.pumpAndSettle();
  return container;
}

Future<void> _selectVersion(WidgetTester tester, String label) async {
  await tester.tap(find.byType(DropdownButton<String>));
  await tester.pumpAndSettle();
  await tester.tap(find.text(label).last);
  await tester.pumpAndSettle();
}

DropdownButton<String> _versionDropdown(WidgetTester tester) =>
    tester.widget(find.byType(DropdownButton<String>));

void main() {
  group('ImageCard', () {
    for (final (image, title, description) in [
      (
        _noble,
        _l10n.imageCardTitleUbuntuServer,
        _l10n.imageCardDescUbuntuServer,
      ),
      (_core24, _l10n.imageCardTitleUbuntuCore, _l10n.imageCardDescUbuntuCore),
      (_debian, _l10n.imageCardTitleDebian, _l10n.imageCardDescDebian),
      (_fedora, _l10n.imageCardTitleFedora, _l10n.imageCardDescFedora),
    ]) {
      testWidgets('shows the title and description for ${image.aliases.first}',
          (tester) async {
        await _pumpCard(tester, parentImage: image);

        expect(find.text(title), findsOneWidget);
        expect(find.text(description), findsOneWidget);
      });
    }

    testWidgets('shows the OS name as the title for an unknown OS',
        (tester) async {
      await _pumpCard(tester, parentImage: _alpine);

      expect(find.text('Alpine'), findsOneWidget);
    });

    for (final (description, image, label) in [
      ('release and codename', _noble, '24.04 LTS (Noble Numbat)'),
      ('only the release when it matches the codename', _fedora, '44'),
      ('only the release when the codename is empty', _alpine, '3.20'),
    ]) {
      testWidgets('labels the version with $description', (tester) async {
        await _pumpCard(tester, parentImage: image);

        expect(find.text(label), findsOneWidget);
      });
    }

    for (final (versions, enabled) in [
      ([_noble], false),
      ([_noble, _jammy], true),
    ]) {
      testWidgets(
          'version dropdown is ${enabled ? 'enabled' : 'disabled'} with '
          '${versions.length} version(s)', (tester) async {
        await _pumpCard(tester, parentImage: _noble, versions: versions);

        expect(_versionDropdown(tester).onChanged != null, enabled);
      });
    }

    testWidgets('selecting a version shows its label', (tester) async {
      await _pumpCard(tester, parentImage: _noble, versions: [_noble, _jammy]);

      await _selectVersion(tester, '22.04 LTS (Jammy Jellyfish)');

      expect(find.text('22.04 LTS (Jammy Jellyfish)'), findsOneWidget);
      expect(find.text('24.04 LTS (Noble Numbat)'), findsNothing);
    });

    testWidgets('Launch launches the selected version with default resources',
        (tester) async {
      final grpcClient = _FakeGrpcClient();
      await _pumpCard(
        tester,
        parentImage: _noble,
        versions: [_noble, _jammy],
        grpcClient: grpcClient,
      );

      await _selectVersion(tester, '22.04 LTS (Jammy Jellyfish)');
      await tester.tap(find.text(_l10n.commonLaunch));

      expect(grpcClient.launchRequests, [
        LaunchRequest(
          instanceName: _vmName,
          image: '22.04',
          numCores: defaultCpus,
          memSize: '${defaultRam}B',
          diskSpace: '${defaultDisk}B',
          remoteName: 'release',
        ),
      ]);
    });

    testWidgets('Launch omits the remote for images without one',
        (tester) async {
      final grpcClient = _FakeGrpcClient();
      await _pumpCard(tester, parentImage: _debian, grpcClient: grpcClient);

      await tester.tap(find.text(_l10n.commonLaunch));

      expect(grpcClient.launchRequests.single.hasRemoteName(), isFalse);
    });

    testWidgets('Configure opens the launch form for the selected version',
        (tester) async {
      final container = await _pumpCard(
        tester,
        parentImage: _noble,
        versions: [_noble, _jammy],
      );

      await _selectVersion(tester, '22.04 LTS (Jammy Jellyfish)');
      await tester.tap(find.text(_l10n.commonConfigure));
      await tester.pumpAndSettle();

      expect(container.read(launchingImageProvider), _jammy);
      expect(tester.state<ScaffoldState>(find.byType(Scaffold)).isEndDrawerOpen,
          isTrue);
    });
  });
}
