import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/sidebar.dart';

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
  group('SidebarKeyNotifier', () {
    ProviderContainer buildContainer(Iterable<String> vmNames) {
      return ProviderContainer(
        overrides: [
          vmNamesProvider.overrideWith((ref) => vmNames.toBuiltSet()),
        ],
      );
    }

    test('initial state is catalogue', () {
      final container = buildContainer([]);
      addTearDown(container.dispose);

      expect(container.read(sidebarKeyProvider), equals('catalogue'));
    });

    test('set updates state to the given key', () {
      final container = buildContainer([]);
      addTearDown(container.dispose);

      container.read(sidebarKeyProvider.notifier).set('vms');

      expect(container.read(sidebarKeyProvider), equals('vms'));
    });

    test('set with a vm key marks vmVisitedProvider as visited', () {
      final container = buildContainer(['myvm']);
      addTearDown(container.dispose);

      container.read(sidebarKeyProvider.notifier).set('vm-myvm');

      expect(container.read(sidebarKeyProvider), equals('vm-myvm'));
      expect(container.read(vmVisitedProvider('vm-myvm')), isTrue);
    });

    test('set with a non-vm key does not affect vmVisitedProvider', () {
      final container = buildContainer([]);
      addTearDown(container.dispose);

      container.read(sidebarKeyProvider.notifier).set('catalogue');

      expect(container.read(vmVisitedProvider('catalogue')), isFalse);
    });

    test('resets to catalogue when current vm is removed from vmNamesProvider',
        () {
      final container = ProviderContainer(
        overrides: [
          vmNamesProvider.overrideWith(
            (ref) => ref.watch(_mutableVmNamesProvider),
          ),
        ],
      );
      addTearDown(container.dispose);

      // Seed with 'foo' so the sidebar key can be set to 'vm-foo'.
      container
          .read(_mutableVmNamesProvider.notifier)
          .setNames(BuiltSet(['foo']));

      // Listen to keep sidebarKeyProvider alive across state changes.
      container.listen(sidebarKeyProvider, (_, __) {});

      container.read(sidebarKeyProvider.notifier).set('vm-foo');
      expect(container.read(sidebarKeyProvider), equals('vm-foo'));

      // Remove 'foo', the notifier should invalidate and reset to 'catalogue'.
      container.read(_mutableVmNamesProvider.notifier).setNames(BuiltSet());

      expect(container.read(sidebarKeyProvider), equals('catalogue'));
    });
  });

  group('VmVisitedNotifier', () {
    test('initial state is false', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      expect(container.read(vmVisitedProvider('vm-test')), isFalse);
    });

    test('setVisited changes state to true', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(vmVisitedProvider('vm-test').notifier).setVisited();

      expect(container.read(vmVisitedProvider('vm-test')), isTrue);
    });

    test('remains true after multiple setVisited calls', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(vmVisitedProvider('vm-test').notifier).setVisited();
      container.read(vmVisitedProvider('vm-test').notifier).setVisited();

      expect(container.read(vmVisitedProvider('vm-test')), isTrue);
    });
  });

  group('SidebarExpandedNotifier', () {
    test('initial state is false', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      expect(container.read(sidebarExpandedProvider), isFalse);
    });

    test('setExpanded(true) updates state to true', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(sidebarExpandedProvider.notifier).setExpanded(true);

      expect(container.read(sidebarExpandedProvider), isTrue);
    });

    test('setExpanded(false) updates state to false', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(sidebarExpandedProvider.notifier).setExpanded(true);
      container.read(sidebarExpandedProvider.notifier).setExpanded(false);

      expect(container.read(sidebarExpandedProvider), isFalse);
    });
  });

  group('SidebarPushContentNotifier', () {
    test('initial state is false', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      expect(container.read(sidebarPushContentProvider), isFalse);
    });

    test('setPushContent(true) updates state to true', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(sidebarPushContentProvider.notifier).setPushContent(true);

      expect(container.read(sidebarPushContentProvider), isTrue);
    });

    test('setPushContent(false) updates state to false', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(sidebarPushContentProvider.notifier).setPushContent(true);
      container.read(sidebarPushContentProvider.notifier).setPushContent(false);

      expect(container.read(sidebarPushContentProvider), isFalse);
    });
  });
}
