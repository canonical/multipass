import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/providers.dart';

class _FakeAllVmInfosNotifier extends AllVmInfosNotifier {
  _FakeAllVmInfosNotifier(this._items);

  final List<DetailedInfoItem> _items;

  @override
  List<DetailedInfoItem> build() => _items;
}

class _FakeLaunchingVmsNotifier extends LaunchingVmsNotifier {
  _FakeLaunchingVmsNotifier(this._items);

  final BuiltList<DetailedInfoItem> _items;

  @override
  BuiltList<DetailedInfoItem> build() => _items;
}

void main() {
  group('daemonAvailableProvider', () {
    test('returns false when ffi is unavailable, regardless of stream state',
        () {
      final container = ProviderContainer(
        overrides: [ffiAvailableProvider.overrideWithValue(false)],
      );
      addTearDown(container.dispose);

      expect(container.read(daemonAvailableProvider), isFalse);
    });

    test('returns true when ffi is available and stream has no error', () {
      final container = ProviderContainer(
        overrides: [
          ffiAvailableProvider.overrideWithValue(true),
          vmInfosStreamProvider.overrideWithValue(const AsyncData([])),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(daemonAvailableProvider), isTrue);
    });

    test(
        'returns false when ffi is available but stream has a GrpcError with '
        'an unrecognised message', () {
      final error = GrpcError.unavailable('some unknown error');
      final container = ProviderContainer(
        overrides: [
          ffiAvailableProvider.overrideWithValue(true),
          vmInfosStreamProvider.overrideWithValue(
            AsyncError(error, StackTrace.empty),
          ),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(daemonAvailableProvider), isFalse);
    });

    test(
        'returns true when ffi is available and stream has the known harmless '
        'GrpcError message', () {
      final error = GrpcError.unavailable(
        'failed to obtain exit status for remote process',
      );
      final container = ProviderContainer(
        overrides: [
          ffiAvailableProvider.overrideWithValue(true),
          vmInfosStreamProvider.overrideWithValue(
            AsyncError(error, StackTrace.empty),
          ),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(daemonAvailableProvider), isTrue);
    });

    test('returns false when ffi is available but stream has a non-GrpcError',
        () {
      final container = ProviderContainer(
        overrides: [
          ffiAvailableProvider.overrideWithValue(true),
          vmInfosStreamProvider.overrideWithValue(
            AsyncError(Exception('some non-grpc error'), StackTrace.empty),
          ),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(daemonAvailableProvider), isFalse);
    });
  });

  group('vmInfosProvider', () {
    ProviderContainer makeContainer({
      required List<DetailedInfoItem> allVmInfos,
      BuiltList<DetailedInfoItem>? launchingVms,
    }) {
      return ProviderContainer(
        overrides: [
          allVmInfosProvider.overrideWith(
            () => _FakeAllVmInfosNotifier(allVmInfos),
          ),
          launchingVmsProvider.overrideWith(
            () => _FakeLaunchingVmsNotifier(launchingVms ?? BuiltList()),
          ),
        ],
      );
    }

    test('excludes DELETED VMs from the result', () {
      final container = makeContainer(
        allVmInfos: [
          DetailedInfoItem(
            name: 'vm-running',
            instanceStatus: InstanceStatus(status: Status.RUNNING),
          ),
          DetailedInfoItem(
            name: 'vm-deleted',
            instanceStatus: InstanceStatus(status: Status.DELETED),
          ),
        ],
      );
      addTearDown(container.dispose);

      final names = container.read(vmInfosProvider).map((v) => v.name).toList();

      expect(names, contains('vm-running'));
      expect(names, isNot(contains('vm-deleted')));
    });

    test('sorts VMs alphabetically by name', () {
      final container = makeContainer(
        allVmInfos: [
          DetailedInfoItem(
            name: 'vm-c',
            instanceStatus: InstanceStatus(status: Status.STOPPED),
          ),
          DetailedInfoItem(
            name: 'vm-a',
            instanceStatus: InstanceStatus(status: Status.RUNNING),
          ),
          DetailedInfoItem(
            name: 'vm-b',
            instanceStatus: InstanceStatus(status: Status.STOPPED),
          ),
        ],
      );
      addTearDown(container.dispose);

      final names = container.read(vmInfosProvider).map((v) => v.name).toList();

      expect(names, equals(['vm-a', 'vm-b', 'vm-c']));
    });

    test('appends launching VMs not already in the existing list', () {
      final container = makeContainer(
        allVmInfos: [
          DetailedInfoItem(
            name: 'vm-a',
            instanceStatus: InstanceStatus(status: Status.RUNNING),
          ),
        ],
        launchingVms: BuiltList([
          DetailedInfoItem(
            name: 'vm-launching',
            instanceStatus: InstanceStatus(status: Status.STARTING),
          ),
        ]),
      );
      addTearDown(container.dispose);

      final names = container.read(vmInfosProvider).map((v) => v.name).toList();

      expect(names, containsAll(['vm-a', 'vm-launching']));
    });

    test(
        'does not duplicate a launching VM already present in the existing list',
        () {
      final container = makeContainer(
        allVmInfos: [
          DetailedInfoItem(
            name: 'vm-a',
            instanceStatus: InstanceStatus(status: Status.RUNNING),
          ),
        ],
        launchingVms: BuiltList([
          DetailedInfoItem(
            name: 'vm-a',
            instanceStatus: InstanceStatus(status: Status.STARTING),
          ),
        ]),
      );
      addTearDown(container.dispose);

      final names = container.read(vmInfosProvider).map((v) => v.name).toList();

      expect(names.where((n) => n == 'vm-a'), hasLength(1));
    });

    test('returns empty list when all inputs are empty', () {
      final container = makeContainer(allVmInfos: []);
      addTearDown(container.dispose);

      expect(container.read(vmInfosProvider), isEmpty);
    });

    test('combines and sorts existing VMs and launching VMs by name', () {
      final container = makeContainer(
        allVmInfos: [
          DetailedInfoItem(
            name: 'vm-b',
            instanceStatus: InstanceStatus(status: Status.STOPPED),
          ),
        ],
        launchingVms: BuiltList([
          DetailedInfoItem(
            name: 'vm-a',
            instanceStatus: InstanceStatus(status: Status.STARTING),
          ),
        ]),
      );
      addTearDown(container.dispose);

      final names = container.read(vmInfosProvider).map((v) => v.name).toList();

      expect(names, equals(['vm-a', 'vm-b']));
    });
  });
}
