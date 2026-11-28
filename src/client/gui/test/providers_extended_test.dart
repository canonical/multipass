import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/providers.dart';
import 'package:shared_preferences/shared_preferences.dart';

class _StaticVmInfosNotifier extends AllVmInfosNotifier {
  _StaticVmInfosNotifier(this._infos);
  final List<DetailedInfoItem> _infos;

  @override
  List<DetailedInfoItem> build() => _infos;
}

/// Build a container with [allVmInfosProvider] overridden to return [infos].
ProviderContainer _containerWithVms(List<DetailedInfoItem> infos) {
  return ProviderContainer(
    overrides: [
      allVmInfosProvider.overrideWith(() => _StaticVmInfosNotifier(infos)),
    ],
  );
}

DetailedInfoItem _vm(String name, Status status) => DetailedInfoItem(
      name: name,
      instanceStatus: InstanceStatus(status: status),
    );

void main() {
  group('deletedVmsProvider', () {
    test('returns empty set when no VMs exist', () {
      final container = _containerWithVms([]);
      addTearDown(container.dispose);

      expect(container.read(deletedVmsProvider), isEmpty);
    });

    test('returns names of DELETED VMs only', () {
      final container = _containerWithVms([
        _vm('running-vm', Status.RUNNING),
        _vm('deleted-vm', Status.DELETED),
        _vm('stopped-vm', Status.STOPPED),
      ]);
      addTearDown(container.dispose);

      expect(
        container.read(deletedVmsProvider),
        equals(BuiltSet<String>(['deleted-vm'])),
      );
    });

    test('includes multiple DELETED VMs', () {
      final container = _containerWithVms([
        _vm('del1', Status.DELETED),
        _vm('del2', Status.DELETED),
        _vm('live', Status.RUNNING),
      ]);
      addTearDown(container.dispose);

      expect(
        container.read(deletedVmsProvider),
        containsAll(['del1', 'del2']),
      );
      expect(container.read(deletedVmsProvider), hasLength(2));
    });

    test('returns empty set when all VMs are non-DELETED', () {
      final container = _containerWithVms([
        _vm('vm1', Status.RUNNING),
        _vm('vm2', Status.STOPPED),
      ]);
      addTearDown(container.dispose);

      expect(container.read(deletedVmsProvider), isEmpty);
    });
  });

  group('trayMenuDataProvider', () {
    test('returns vmStatusesProvider when daemon is available', () {
      final statuses = BuiltMap<String, Status>({
        'vm1': Status.RUNNING,
        'vm2': Status.STOPPED,
      });

      final container = ProviderContainer(
        overrides: [
          daemonAvailableProvider.overrideWithValue(true),
          vmStatusesProvider.overrideWithValue(statuses),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(trayMenuDataProvider), equals(statuses));
    });

    test('returns null when daemon is not available', () {
      final container = ProviderContainer(
        overrides: [
          daemonAvailableProvider.overrideWithValue(false),
          vmStatusesProvider.overrideWithValue(BuiltMap()),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(trayMenuDataProvider), isNull);
    });
  });

  group('SessionTerminalFontSizeNotifier', () {
    test('initial state is the default font size (13.0)', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      expect(
        container.read(sessionTerminalFontSizeProvider),
        equals(SessionTerminalFontSizeNotifier.defaultFontSize),
      );
    });

    test('set() updates the font size', () {
      final container = ProviderContainer();
      addTearDown(container.dispose);

      container.read(sessionTerminalFontSizeProvider.notifier).set(16.0);

      expect(container.read(sessionTerminalFontSizeProvider), equals(16.0));
    });
  });

  group('GuiSettingNotifier', () {
    setUp(() {
      SharedPreferences.setMockInitialValues({});
    });

    test('returns stored value when key is present in SharedPreferences',
        () async {
      SharedPreferences.setMockInitialValues({'hotkey': 'ctrl+h'});
      final prefs = await SharedPreferences.getInstance();

      final container = ProviderContainer(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(guiSettingProvider('hotkey')), equals('ctrl+h'));
    });

    test('returns default "ask" for onAppCloseKey when not stored', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();

      final container = ProviderContainer(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(guiSettingProvider(onAppCloseKey)), equals('ask'));
    });

    test('returns null for unknown key with no stored value', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();

      final container = ProviderContainer(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
      );
      addTearDown(container.dispose);

      expect(container.read(guiSettingProvider('unknown-key')), isNull);
    });

    test('set() persists to SharedPreferences and updates state', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();

      final container = ProviderContainer(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
      );
      addTearDown(container.dispose);
      container.listen(guiSettingProvider('hotkey'), (_, __) {});

      container.read(guiSettingProvider('hotkey').notifier).set('ctrl+k');

      expect(container.read(guiSettingProvider('hotkey')), equals('ctrl+k'));
      expect(prefs.getString('hotkey'), equals('ctrl+k'));
    });

    test('updateShouldNotify returns true when value changes', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();

      final container = ProviderContainer(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
      );
      addTearDown(container.dispose);

      final notifier = container.read(guiSettingProvider('k').notifier);
      expect(notifier.updateShouldNotify('old', 'new'), isTrue);
    });

    test('updateShouldNotify returns false when value is unchanged', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();

      final container = ProviderContainer(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
      );
      addTearDown(container.dispose);

      final notifier = container.read(guiSettingProvider('k').notifier);
      expect(notifier.updateShouldNotify('same', 'same'), isFalse);
    });
  });
}
