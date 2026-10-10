import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/catalogue.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/grpc_client.dart';

// Shared ImageInfo fixtures
final noble = ImageInfo(
    os: 'Ubuntu',
    release: '24.04',
    codename: 'noble',
    aliases: ['24.04', 'lts']);
final jammy = ImageInfo(
    os: 'Ubuntu', release: '22.04', codename: 'jammy', aliases: ['22.04']);
final focal = ImageInfo(
    os: 'Ubuntu', release: '20.04', codename: 'focal', aliases: ['20.04']);
final mantic = ImageInfo(
    os: 'Ubuntu', release: '23.10', codename: 'mantic', aliases: ['23.10']);
final plucky = ImageInfo(
    os: 'Ubuntu',
    release: '25.04',
    codename: 'plucky',
    aliases: ['25.04', 'devel']);
final core22 =
    ImageInfo(os: 'Ubuntu', release: '22', codename: '', aliases: ['core22']);
final core18 =
    ImageInfo(os: 'Ubuntu', release: '18', codename: '', aliases: ['core18']);
final core16 =
    ImageInfo(os: 'Ubuntu', release: '16', codename: '', aliases: ['core16']);
final centos8 =
    ImageInfo(os: 'CentOS', release: '8', codename: '', aliases: ['centos8']);
final centos7 =
    ImageInfo(os: 'CentOS', release: '7', codename: '', aliases: ['centos7']);
final alpine =
    ImageInfo(os: 'Alpine', release: '3.20', codename: '', aliases: ['alpine']);

void main() {
  group('sortImages', () {
    test('returns empty list for empty input', () {
      expect(sortImages([]), isEmpty);
    });

    test('returns single item list unchanged', () {
      final result = sortImages([jammy]);
      expect(result.length, equals(1));
      expect(result.first.release, equals('22.04'));
    });

    test('LTS image comes first when present', () {
      final result = sortImages([jammy, noble, focal]);
      expect(result.first.aliases, contains('lts'));
    });

    test('Ubuntu releases are sorted newest-first after LTS', () {
      final result = sortImages([focal, noble, jammy, mantic]);
      // lts first
      expect(result[0].aliases, contains('lts'));
      // then newest non-lts Ubuntu releases
      expect(result[1].release, equals('23.10'));
      expect(result[2].release, equals('22.04'));
      expect(result[3].release, equals('20.04'));
    });

    test('devel image comes after Ubuntu releases', () {
      final result = sortImages([noble, jammy, plucky]);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].aliases, contains('devel'));
    });

    test('core images come after devel', () {
      final result = sortImages([noble, plucky, core22, core18]);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].aliases, contains('devel'));
      expect(result[2].aliases, contains('core22'));
      expect(result[3].aliases, contains('core18'));
    });

    test('third-party (non-Ubuntu) images come last', () {
      // Uses alias 'centos' (not 'centos8') to match the real multipass alias.
      final centos = ImageInfo(
          os: 'CentOS', release: '8', codename: '', aliases: ['centos']);
      final result = sortImages([noble, centos, jammy]);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].os, equals('CentOS'));
    });

    test('no LTS at front when LTS is absent', () {
      final result = sortImages([jammy, mantic]);
      expect(result.first.aliases, isNot(contains('lts')));
      expect(result[0].release, equals('23.10'));
      expect(result[1].release, equals('22.04'));
    });

    test('no devel after Ubuntu images when devel is absent', () {
      final result = sortImages([noble, jammy, core22]);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].aliases, contains('core22'));
      expect(result.any((i) => i.aliases.contains('devel')), isFalse);
    });

    test('third-party images are sorted newest-first among themselves', () {
      final result = sortImages([centos7, alpine, centos8]);
      final releases = result.map((i) => i.release).toList();
      // Sorting is lexicographic (string compareTo), so '8' > '7' > '3.20'
      expect(releases, equals(['8', '7', '3.20']));
    });

    test('full ordering: LTS, Ubuntu releases, devel, core, third-party', () {
      final result = sortImages([core16, alpine, plucky, jammy, noble, core22]);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].aliases, contains('devel'));
      expect(result[3].aliases, contains('core22'));
      expect(result[4].aliases, contains('core16'));
      expect(result[5].os, equals('Alpine'));
    });
  });

  group('imageName', () {
    test('combines os, release, and codename for non-core image', () {
      expect(imageName(noble), equals('Ubuntu 24.04 noble'));
    });

    test('omits codename for image with alias "core"', () {
      // focal with the bare 'core' alias — distinct from the standard focal fixture.
      final focalCore = ImageInfo(
        os: 'Ubuntu',
        release: '20.04',
        codename: 'focal',
        aliases: ['20.04', 'core'],
      );
      expect(imageName(focalCore), equals('Ubuntu 20.04'));
    });

    test('omits codename for image with alias "core18"', () {
      expect(imageName(core18), equals('Ubuntu 18'));
    });

    test('omits codename for image with alias "core22"', () {
      expect(imageName(core22), equals('Ubuntu 22'));
    });

    test('includes codename for non-core devel image', () {
      expect(imageName(plucky), equals('Ubuntu 25.04 plucky'));
    });
  });

  group('SelectedImageNotifier', () {
    test('initial state is null', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(selectedImageProvider('key1'), (_, __) {});

      expect(container.read(selectedImageProvider('key1')), isNull);
    });

    test('set(image) updates state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(selectedImageProvider('key1'), (_, __) {});

      container.read(selectedImageProvider('key1').notifier).set(noble);

      expect(container.read(selectedImageProvider('key1')), equals(noble));
    });

    test('set(image) replaces previous state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(selectedImageProvider('key1'), (_, __) {});

      container.read(selectedImageProvider('key1').notifier).set(jammy);
      container.read(selectedImageProvider('key1').notifier).set(noble);

      expect(container.read(selectedImageProvider('key1')), equals(noble));
    });

    test('different keys have independent state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(selectedImageProvider('key1'), (_, __) {});
      container.listen(selectedImageProvider('key2'), (_, __) {});

      container.read(selectedImageProvider('key1').notifier).set(noble);

      expect(container.read(selectedImageProvider('key1')), equals(noble));
      expect(container.read(selectedImageProvider('key2')), isNull);
    });
  });
}
