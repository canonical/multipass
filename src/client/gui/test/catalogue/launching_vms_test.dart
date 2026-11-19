import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/providers.dart';

void main() {
  group('LaunchingVmsNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    BuiltList<DetailedInfoItem> state() => container.read(launchingVmsProvider);

    LaunchingVmsNotifier notifier() =>
        container.read(launchingVmsProvider.notifier);

    test('initial state is an empty BuiltList', () {
      expect(state(), isEmpty);
      expect(state(), isA<BuiltList<DetailedInfoItem>>());
    });

    test('add() appends a DetailedInfoItem with correct fields', () {
      final request = LaunchRequest(
        instanceName: 'my-vm',
        numCores: 2,
        diskSpace: '10G',
        memSize: '4G',
        image: 'ubuntu',
      );

      notifier().add(request);

      expect(state(), hasLength(1));
      final item = state().first;
      expect(item.name, equals('my-vm'));
      expect(item.cpuCount, equals('2'));
      expect(item.diskTotal, equals('10G'));
      expect(item.memoryTotal, equals('4G'));
      expect(item.instanceInfo.currentRelease, equals('ubuntu'));
    });

    test('add() called twice appends both items', () {
      final request1 = LaunchRequest(
        instanceName: 'vm-one',
        numCores: 1,
        diskSpace: '5G',
        memSize: '1G',
        image: 'ubuntu',
      );
      final request2 = LaunchRequest(
        instanceName: 'vm-two',
        numCores: 4,
        diskSpace: '20G',
        memSize: '8G',
        image: 'noble',
      );

      notifier().add(request1);
      notifier().add(request2);

      expect(state(), hasLength(2));
      expect(state()[0].name, equals('vm-one'));
      expect(state()[1].name, equals('vm-two'));
    });

    test('remove() removes the item with the given name', () {
      notifier().add(
        LaunchRequest(
          instanceName: 'target-vm',
          numCores: 2,
          diskSpace: '10G',
          memSize: '4G',
          image: 'ubuntu',
        ),
      );
      expect(state(), hasLength(1));

      notifier().remove('target-vm');

      expect(state(), isEmpty);
    });

    test('remove() with nonexistent name leaves list unchanged', () {
      notifier().add(
        LaunchRequest(
          instanceName: 'existing-vm',
          numCores: 2,
          diskSpace: '10G',
          memSize: '4G',
          image: 'ubuntu',
        ),
      );
      expect(state(), hasLength(1));

      notifier().remove('nonexistent');

      expect(state(), hasLength(1));
      expect(state().first.name, equals('existing-vm'));
    });

    test('add() then remove() leaves an empty list', () {
      notifier().add(
        LaunchRequest(
          instanceName: 'temp-vm',
          numCores: 2,
          diskSpace: '10G',
          memSize: '4G',
          image: 'ubuntu',
        ),
      );
      notifier().remove('temp-vm');

      expect(state(), isEmpty);
    });

    test('updateShouldNotify returns true when lists differ', () {
      final list1 = BuiltList<DetailedInfoItem>();
      final list2 = BuiltList<DetailedInfoItem>([
        DetailedInfoItem(name: 'vm1'),
      ]);

      expect(notifier().updateShouldNotify(list1, list2), isTrue);
    });

    test('updateShouldNotify returns false for the same list object', () {
      final list1 = BuiltList<DetailedInfoItem>();

      expect(notifier().updateShouldNotify(list1, list1), isFalse);
    });
  });

  group('isLaunchingProvider', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    LaunchingVmsNotifier notifier() =>
        container.read(launchingVmsProvider.notifier);

    test('returns false when launchingVmsProvider is empty', () {
      container.listen(isLaunchingProvider('vm1'), (_, __) {});

      expect(container.read(isLaunchingProvider('vm1')), isFalse);
    });

    test('returns true after adding a vm with that name', () {
      container.listen(isLaunchingProvider('vm1'), (_, __) {});

      notifier().add(
        LaunchRequest(
          instanceName: 'vm1',
          numCores: 2,
          diskSpace: '10G',
          memSize: '4G',
          image: 'ubuntu',
        ),
      );

      expect(container.read(isLaunchingProvider('vm1')), isTrue);
    });

    test('returns false for a different vm name', () {
      container.listen(isLaunchingProvider('vm1'), (_, __) {});
      container.listen(isLaunchingProvider('vm2'), (_, __) {});

      notifier().add(
        LaunchRequest(
          instanceName: 'vm1',
          numCores: 2,
          diskSpace: '10G',
          memSize: '4G',
          image: 'ubuntu',
        ),
      );

      expect(container.read(isLaunchingProvider('vm1')), isTrue);
      expect(container.read(isLaunchingProvider('vm2')), isFalse);
    });

    test('returns false again after removing the vm', () {
      container.listen(isLaunchingProvider('vm1'), (_, __) {});

      notifier().add(
        LaunchRequest(
          instanceName: 'vm1',
          numCores: 2,
          diskSpace: '10G',
          memSize: '4G',
          image: 'ubuntu',
        ),
      );
      expect(container.read(isLaunchingProvider('vm1')), isTrue);

      notifier().remove('vm1');

      expect(container.read(isLaunchingProvider('vm1')), isFalse);
    });
  });
}
