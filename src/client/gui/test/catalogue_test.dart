import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/catalogue.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/grpc_client.dart';

void main() {
  group('sortImages', () {
    test('returns empty list for empty input', () {
      expect(sortImages([]), isEmpty);
    });

    test('returns single item list unchanged', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
      ];
      final result = sortImages(images);
      expect(result.length, equals(1));
      expect(result.first.release, equals('22.04'));
    });

    test('LTS image comes first when present', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'Ubuntu',
            release: '20.04',
            codename: 'focal',
            aliases: ['20.04']),
      ];
      final result = sortImages(images);
      expect(result.first.aliases, contains('lts'));
    });

    test('Ubuntu releases are sorted newest-first after LTS', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '20.04',
            codename: 'focal',
            aliases: ['20.04']),
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
        ImageInfo(
            os: 'Ubuntu',
            release: '23.10',
            codename: 'mantic',
            aliases: ['23.10']),
      ];
      final result = sortImages(images);
      // lts first
      expect(result[0].aliases, contains('lts'));
      // then newest non-lts Ubuntu releases
      expect(result[1].release, equals('23.10'));
      expect(result[2].release, equals('22.04'));
      expect(result[3].release, equals('20.04'));
    });

    test('devel image comes after Ubuntu releases', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
        ImageInfo(
            os: 'Ubuntu',
            release: '25.04',
            codename: 'plucky',
            aliases: ['25.04', 'devel']),
      ];
      final result = sortImages(images);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].aliases, contains('devel'));
    });

    test('core images come after devel', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'Ubuntu',
            release: '25.04',
            codename: 'plucky',
            aliases: ['25.04', 'devel']),
        ImageInfo(
            os: 'Ubuntu', release: '22', codename: '', aliases: ['core22']),
        ImageInfo(
            os: 'Ubuntu', release: '18', codename: '', aliases: ['core18']),
      ];
      final result = sortImages(images);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].aliases, contains('devel'));
      expect(result[2].aliases, contains('core22'));
      expect(result[3].aliases, contains('core18'));
    });

    test('third-party (non-Ubuntu) images come last', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'CentOS', release: '8', codename: '', aliases: ['centos']),
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
      ];
      final result = sortImages(images);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].os, equals('CentOS'));
    });

    test('no LTS at front when LTS is absent', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
        ImageInfo(
            os: 'Ubuntu',
            release: '23.10',
            codename: 'mantic',
            aliases: ['23.10']),
      ];
      final result = sortImages(images);
      expect(result.first.aliases, isNot(contains('lts')));
      expect(result[0].release, equals('23.10'));
      expect(result[1].release, equals('22.04'));
    });

    test('no devel after Ubuntu images when devel is absent', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
        ImageInfo(
            os: 'Ubuntu', release: '22', codename: '', aliases: ['core22']),
      ];
      final result = sortImages(images);
      expect(result[0].aliases, contains('lts'));
      expect(result[1].release, equals('22.04'));
      expect(result[2].aliases, contains('core22'));
      expect(result.any((i) => i.aliases.contains('devel')), isFalse);
    });

    test('third-party images are sorted newest-first among themselves', () {
      final images = [
        ImageInfo(
            os: 'CentOS', release: '7', codename: '', aliases: ['centos7']),
        ImageInfo(
            os: 'Alpine', release: '3.20', codename: '', aliases: ['alpine']),
        ImageInfo(
            os: 'CentOS', release: '8', codename: '', aliases: ['centos8']),
      ];
      final result = sortImages(images);
      final releases = result.map((i) => i.release).toList();
      // Sorting is lexicographic (string compareTo), so '8' > '7' > '3.20'
      expect(releases, equals(['8', '7', '3.20']));
    });

    test('full ordering: LTS, Ubuntu releases, devel, core, third-party', () {
      final images = [
        ImageInfo(
            os: 'Ubuntu', release: '16', codename: '', aliases: ['core16']),
        ImageInfo(
            os: 'Alpine', release: '3.20', codename: '', aliases: ['alpine']),
        ImageInfo(
            os: 'Ubuntu',
            release: '25.04',
            codename: 'plucky',
            aliases: ['25.04', 'devel']),
        ImageInfo(
            os: 'Ubuntu',
            release: '22.04',
            codename: 'jammy',
            aliases: ['22.04']),
        ImageInfo(
            os: 'Ubuntu',
            release: '24.04',
            codename: 'noble',
            aliases: ['24.04', 'lts']),
        ImageInfo(
            os: 'Ubuntu', release: '22', codename: '', aliases: ['core22']),
      ];
      final result = sortImages(images);
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
      final image = ImageInfo(
        os: 'Ubuntu',
        release: '24.04',
        codename: 'noble',
        aliases: ['24.04', 'lts'],
      );
      expect(imageName(image), equals('Ubuntu 24.04 noble'));
    });

    test('omits codename for image with alias "core"', () {
      final image = ImageInfo(
        os: 'Ubuntu',
        release: '20.04',
        codename: 'focal',
        aliases: ['20.04', 'core'],
      );
      expect(imageName(image), equals('Ubuntu 20.04'));
    });

    test('omits codename for image with alias "core18"', () {
      final image = ImageInfo(
        os: 'Ubuntu',
        release: '18',
        codename: '',
        aliases: ['core18'],
      );
      expect(imageName(image), equals('Ubuntu 18'));
    });

    test('omits codename for image with alias "core22"', () {
      final image = ImageInfo(
        os: 'Ubuntu',
        release: '22',
        codename: '',
        aliases: ['core22'],
      );
      expect(imageName(image), equals('Ubuntu 22'));
    });

    test('includes codename for non-core devel image', () {
      final image = ImageInfo(
        os: 'Ubuntu',
        release: '25.04',
        codename: 'plucky',
        aliases: ['25.04', 'devel'],
      );
      expect(imageName(image), equals('Ubuntu 25.04 plucky'));
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

      final image = ImageInfo(
        os: 'Ubuntu',
        release: '24.04',
        codename: 'noble',
        aliases: ['24.04', 'lts'],
      );
      container.read(selectedImageProvider('key1').notifier).set(image);

      expect(container.read(selectedImageProvider('key1')), equals(image));
    });

    test('set(image) replaces previous state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(selectedImageProvider('key1'), (_, __) {});

      final first = ImageInfo(
        os: 'Ubuntu',
        release: '22.04',
        codename: 'jammy',
        aliases: ['22.04'],
      );
      final second = ImageInfo(
        os: 'Ubuntu',
        release: '24.04',
        codename: 'noble',
        aliases: ['24.04', 'lts'],
      );
      container.read(selectedImageProvider('key1').notifier).set(first);
      container.read(selectedImageProvider('key1').notifier).set(second);

      expect(container.read(selectedImageProvider('key1')), equals(second));
    });

    test('different keys have independent state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(selectedImageProvider('key1'), (_, __) {});
      container.listen(selectedImageProvider('key2'), (_, __) {});

      final image = ImageInfo(
        os: 'Ubuntu',
        release: '24.04',
        codename: 'noble',
        aliases: ['24.04', 'lts'],
      );
      container.read(selectedImageProvider('key1').notifier).set(image);

      expect(container.read(selectedImageProvider('key1')), equals(image));
      expect(container.read(selectedImageProvider('key2')), isNull);
    });
  });
}
