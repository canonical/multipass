import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_details.dart';

class _StatusNotifier extends Notifier<Status> {
  @override
  Status build() => Status.STOPPED;

  void set(Status value) => state = value;
}

final _vmStatusProvider =
    NotifierProvider<_StatusNotifier, Status>(_StatusNotifier.new);

void main() {
  group('VmScreenLocationNotifier', () {
    test('initial state is shells', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(vmScreenLocationProvider('vm1'), (_, __) {});

      expect(
        container.read(vmScreenLocationProvider('vm1')),
        equals(VmDetailsLocation.shells),
      );
    });

    test('set(details) updates state to details', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(vmScreenLocationProvider('vm1'), (_, __) {});

      container
          .read(vmScreenLocationProvider('vm1').notifier)
          .set(VmDetailsLocation.details);

      expect(
        container.read(vmScreenLocationProvider('vm1')),
        equals(VmDetailsLocation.details),
      );
    });

    test('set(shells) updates state to shells', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(vmScreenLocationProvider('vm1'), (_, __) {});

      container
          .read(vmScreenLocationProvider('vm1').notifier)
          .set(VmDetailsLocation.details);
      container
          .read(vmScreenLocationProvider('vm1').notifier)
          .set(VmDetailsLocation.shells);

      expect(
        container.read(vmScreenLocationProvider('vm1')),
        equals(VmDetailsLocation.shells),
      );
    });

    test('different vm names have independent state', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(vmScreenLocationProvider('vm1'), (_, __) {});
      container.listen(vmScreenLocationProvider('vm2'), (_, __) {});

      container
          .read(vmScreenLocationProvider('vm1').notifier)
          .set(VmDetailsLocation.details);

      expect(
        container.read(vmScreenLocationProvider('vm1')),
        equals(VmDetailsLocation.details),
      );
      expect(
        container.read(vmScreenLocationProvider('vm2')),
        equals(VmDetailsLocation.shells),
      );
    });
  });

  group('ActiveEditPageNotifier', () {
    ProviderContainer buildContainer() {
      final container = ProviderContainer(
        overrides: [
          vmInfoProvider('test-vm').overrideWithBuild(
            (ref, notifier) => DetailedInfoItem(
              instanceStatus:
                  InstanceStatus(status: ref.watch(_vmStatusProvider)),
            ),
          ),
        ],
      );
      addTearDown(container.dispose);
      container.listen(activeEditPageProvider('test-vm'), (_, __) {});
      return container;
    }

    test('initial state is null', () {
      final container = buildContainer();

      expect(container.read(activeEditPageProvider('test-vm')), isNull);
    });

    test('set(resources) updates state to resources', () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.resources);

      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.resources),
      );
    });

    test('set(bridge) updates state to bridge', () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.bridge);

      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.bridge),
      );
    });

    test('set(mounts) updates state to mounts', () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.mounts);

      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.mounts),
      );
    });

    test('set(null) updates state to null', () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.resources);
      container.read(activeEditPageProvider('test-vm').notifier).set(null);

      expect(container.read(activeEditPageProvider('test-vm')), isNull);
    });

    test('resets to null when in bridge state and vm status becomes RUNNING',
        () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.bridge);
      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.bridge),
      );

      container.read(_vmStatusProvider.notifier).set(Status.RUNNING);

      expect(container.read(activeEditPageProvider('test-vm')), isNull);
    });

    test('resets to null when in resources state and vm status becomes RUNNING',
        () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.resources);
      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.resources),
      );

      container.read(_vmStatusProvider.notifier).set(Status.RUNNING);

      expect(container.read(activeEditPageProvider('test-vm')), isNull);
    });

    test('does not reset when in mounts state and vm status becomes RUNNING',
        () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.mounts);
      container.read(_vmStatusProvider.notifier).set(Status.RUNNING);

      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.mounts),
      );
    });

    test('does not reset when in bridge state and vm status changes to STOPPED',
        () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.bridge);
      container.read(_vmStatusProvider.notifier).set(Status.STOPPED);

      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.bridge),
      );
    });

    test(
        'does not reset when in resources state and vm status changes to STOPPED',
        () {
      final container = buildContainer();

      container
          .read(activeEditPageProvider('test-vm').notifier)
          .set(ActiveEditPage.resources);
      container.read(_vmStatusProvider.notifier).set(Status.STOPPED);

      expect(
        container.read(activeEditPageProvider('test-vm')),
        equals(ActiveEditPage.resources),
      );
    });
  });
}
