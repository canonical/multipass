import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/providers.dart';

import 'image_fixtures.dart';

void main() {
  group('LaunchingImageNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    ImageInfo state() => container.read(launchingImageProvider);
    LaunchingImageNotifier notifier() =>
        container.read(launchingImageProvider.notifier);

    test('initial state is an empty image', () {
      expect(state(), ImageInfo());
    });

    test('set() replaces the launching image', () {
      notifier().set(jammy);
      notifier().set(noble);
      expect(state(), same(noble));
    });
  });

  group('imageName', () {
    for (final (description, image, name) in [
      ('appends the codename', noble, 'Ubuntu 24.04 LTS Noble Numbat'),
      ('omits the codename when it matches the release', fedora, 'Fedora 44'),
      ('omits the codename when it is empty', alpine, 'Alpine 3.20'),
      ('omits the codename for Core images', core24, 'Ubuntu Core 24'),
    ]) {
      test(description, () {
        expect(imageName(image), name);
      });
    }
  });
}
