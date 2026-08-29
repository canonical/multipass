import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_table/header_selection.dart';
import 'package:multipass_gui/vm_table/search_box.dart';
import 'package:multipass_gui/vm_table/vms.dart';

class _MutableVmNamesNotifier extends Notifier<BuiltSet<String>> {
  @override
  BuiltSet<String> build() => BuiltSet();

  void setNames(BuiltSet<String> names) {
    state = names;
  }
}

final _mutableVmNamesProvider =
    NotifierProvider<_MutableVmNamesNotifier, BuiltSet<String>>(
  _MutableVmNamesNotifier.new,
);

void main() {
  group('SearchNameNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    test('initial state is empty string', () {
      expect(container.read(searchNameProvider), equals(''));
    });

    test('set updates state to given value', () {
      container.read(searchNameProvider.notifier).set('hello');
      expect(container.read(searchNameProvider), equals('hello'));
    });

    test('set with empty string resets to empty', () {
      container.read(searchNameProvider.notifier).set('hello');
      container.read(searchNameProvider.notifier).set('');
      expect(container.read(searchNameProvider), equals(''));
    });
  });

  group('RunningOnlyNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    test('initial state is false', () {
      expect(container.read(runningOnlyProvider), isFalse);
    });

    test('set true updates state to true', () {
      container.read(runningOnlyProvider.notifier).set(true);
      expect(container.read(runningOnlyProvider), isTrue);
    });

    test('set false after true resets to false', () {
      container.read(runningOnlyProvider.notifier).set(true);
      container.read(runningOnlyProvider.notifier).set(false);
      expect(container.read(runningOnlyProvider), isFalse);
    });
  });

  group('SelectedVmsNotifier', () {
    ProviderContainer makeContainer({
      List<String> vmNames = const [],
    }) {
      return ProviderContainer(
        overrides: [
          vmNamesProvider.overrideWith(
            (ref) => BuiltSet<String>(vmNames),
          ),
        ],
      );
    }

    test('initial state is empty BuiltSet', () {
      final container = makeContainer();
      addTearDown(container.dispose);
      expect(container.read(selectedVmsProvider), equals(BuiltSet<String>()));
    });

    test('toggle true adds vm name to the set', () {
      final container = makeContainer(vmNames: ['vm1', 'vm2']);
      addTearDown(container.dispose);

      container.read(selectedVmsProvider.notifier).toggle('vm1', true);
      expect(container.read(selectedVmsProvider), equals(BuiltSet(['vm1'])));
    });

    test('toggle false removes vm name from the set', () {
      final container = makeContainer(vmNames: ['vm1', 'vm2']);
      addTearDown(container.dispose);

      container.read(selectedVmsProvider.notifier).toggle('vm1', true);
      container.read(selectedVmsProvider.notifier).toggle('vm2', true);
      container.read(selectedVmsProvider.notifier).toggle('vm1', false);
      expect(container.read(selectedVmsProvider), equals(BuiltSet(['vm2'])));
    });

    test('set replaces entire state', () {
      final container = makeContainer(vmNames: ['vm1', 'vm2']);
      addTearDown(container.dispose);

      container
          .read(selectedVmsProvider.notifier)
          .set(BuiltSet(['vm1', 'vm2']));
      expect(
        container.read(selectedVmsProvider),
        equals(BuiltSet(['vm1', 'vm2'])),
      );
    });

    test('vm removed from vmNamesProvider is auto-removed from selection', () {
      final container = ProviderContainer(
        overrides: [
          vmNamesProvider.overrideWith(
            (ref) => ref.watch(_mutableVmNamesProvider),
          ),
        ],
      );
      addTearDown(container.dispose);

      container.read(_mutableVmNamesProvider.notifier).setNames(
            BuiltSet(['vm1', 'vm2']),
          );
      container.listen(selectedVmsProvider, (_, __) {});

      container.read(selectedVmsProvider.notifier).toggle('vm1', true);
      container.read(selectedVmsProvider.notifier).toggle('vm2', true);
      expect(
        container.read(selectedVmsProvider),
        equals(BuiltSet(['vm1', 'vm2'])),
      );

      container.read(_mutableVmNamesProvider.notifier).setNames(
            BuiltSet(['vm2']),
          );

      expect(container.read(selectedVmsProvider), equals(BuiltSet(['vm2'])));
    });

    test('changing runningOnlyProvider resets selected vms to empty', () {
      final container = makeContainer(vmNames: ['vm1', 'vm2']);
      addTearDown(container.dispose);

      container
          .read(selectedVmsProvider.notifier)
          .set(BuiltSet(['vm1', 'vm2']));
      expect(container.read(selectedVmsProvider).isNotEmpty, isTrue);

      container.read(runningOnlyProvider.notifier).set(true);
      expect(container.read(selectedVmsProvider), equals(BuiltSet<String>()));
    });

    test('changing searchNameProvider resets selected vms to empty', () {
      final container = makeContainer(vmNames: ['vm1', 'vm2']);
      addTearDown(container.dispose);

      container
          .read(selectedVmsProvider.notifier)
          .set(BuiltSet(['vm1', 'vm2']));
      expect(container.read(selectedVmsProvider).isNotEmpty, isTrue);

      container.read(searchNameProvider.notifier).set('filter');
      expect(container.read(selectedVmsProvider), equals(BuiltSet<String>()));
    });
  });

  group('EnabledHeadersNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
    });

    tearDown(() {
      container.dispose();
    });

    test('initial state has all headers enabled', () {
      final state = container.read(enabledHeadersProvider);
      expect(state.values, everyElement(isTrue));
    });

    test('toggleHeader false disables the named header', () {
      container.read(enabledHeadersProvider.notifier).toggleHeader(
            'STATE',
            false,
          );
      expect(container.read(enabledHeadersProvider)['STATE'], isFalse);
    });

    test('toggleHeader true re-enables a disabled header', () {
      container.read(enabledHeadersProvider.notifier).toggleHeader(
            'STATE',
            false,
          );
      container.read(enabledHeadersProvider.notifier).toggleHeader(
            'STATE',
            true,
          );
      expect(container.read(enabledHeadersProvider)['STATE'], isTrue);
    });

    test('toggling one header does not affect others', () {
      container.read(enabledHeadersProvider.notifier).toggleHeader(
            'STATE',
            false,
          );
      final state = container.read(enabledHeadersProvider);
      for (final entry in state.entries) {
        if (entry.key != 'STATE') {
          expect(entry.value, isTrue,
              reason: '${entry.key} should remain enabled');
        }
      }
    });
  });
}
