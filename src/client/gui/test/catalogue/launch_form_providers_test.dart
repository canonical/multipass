import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/launch_form.dart';
import 'package:multipass_gui/grpc_client.dart';

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

    test('initial state is an empty ImageInfo', () {
      expect(state(), equals(ImageInfo()));
    });

    test('set() updates state to the given ImageInfo', () {
      final image = ImageInfo(
        os: 'Ubuntu',
        release: '24.04',
        codename: 'noble',
        aliases: ['24.04', 'lts'],
      );

      container.read(launchingImageProvider.notifier).set(image);

      expect(state(), equals(image));
    });

    test('set() called twice replaces state with the latest value', () {
      final first =
          ImageInfo(os: 'Ubuntu', release: '22.04', aliases: ['22.04']);
      final second =
          ImageInfo(os: 'Ubuntu', release: '24.04', aliases: ['24.04']);

      container.read(launchingImageProvider.notifier).set(first);
      container.read(launchingImageProvider.notifier).set(second);

      expect(state(), equals(second));
    });
  });
}
