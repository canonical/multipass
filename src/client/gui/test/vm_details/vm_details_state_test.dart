import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details.dart';

const _vmName = 'test-vm';

void main() {
  group('VmScreenLocationNotifier', () {
    ProviderContainer buildContainer() {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(vmScreenLocationProvider('vm1'), (_, __) {});
      container.listen(vmScreenLocationProvider('vm2'), (_, __) {});
      return container;
    }

    test('starts on shells', () {
      final container = buildContainer();

      expect(
        container.read(vmScreenLocationProvider('vm1')),
        VmDetailsLocation.shells,
      );
    });

    test('keeps a separate location per instance', () {
      final container = buildContainer();

      container
          .read(vmScreenLocationProvider('vm1').notifier)
          .set(VmDetailsLocation.details);

      expect(
        container.read(vmScreenLocationProvider('vm1')),
        VmDetailsLocation.details,
      );
      expect(
        container.read(vmScreenLocationProvider('vm2')),
        VmDetailsLocation.shells,
      );
    });
  });

  group('ActiveEditPageNotifier', () {
    ProviderContainer buildContainer([ActiveEditPage? page]) {
      final container = ProviderContainer(
        overrides: [
          vmInfoProvider(_vmName).overrideWithBuild(
            (ref, _) => DetailedInfoItem(
              instanceStatus: InstanceStatus(status: Status.STOPPED),
            ),
          ),
        ],
      );
      addTearDown(container.dispose);
      container.listen(activeEditPageProvider(_vmName), (_, __) {});
      container.read(activeEditPageProvider(_vmName).notifier).set(page);
      return container;
    }

    void setStatus(ProviderContainer container, Status status) {
      container.read(vmInfoProvider(_vmName).notifier).state =
          DetailedInfoItem(instanceStatus: InstanceStatus(status: status));
    }

    test('starts with no active page', () {
      final container = buildContainer();

      expect(container.read(activeEditPageProvider(_vmName)), isNull);
    });

    for (final page in [ActiveEditPage.bridge, ActiveEditPage.resources]) {
      for (final status in [
        Status.RUNNING,
        Status.SUSPENDED,
        Status.UNAVAILABLE,
      ]) {
        test('clears ${page.name} when status becomes ${status.name}', () {
          final container = buildContainer(page);

          setStatus(container, status);

          expect(container.read(activeEditPageProvider(_vmName)), isNull);
        });
      }
    }

    test('keeps mounts when status becomes RUNNING', () {
      final container = buildContainer(ActiveEditPage.mounts);

      setStatus(container, Status.RUNNING);

      expect(
        container.read(activeEditPageProvider(_vmName)),
        ActiveEditPage.mounts,
      );
    });

    test('clears mounts when status becomes UNAVAILABLE', () {
      final container = buildContainer(ActiveEditPage.mounts);

      setStatus(container, Status.UNAVAILABLE);

      expect(container.read(activeEditPageProvider(_vmName)), isNull);
    });
  });
}
