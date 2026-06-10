import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/sidebar.dart';
import 'package:multipass_gui/vm_table/vm_table_headers.dart';
import 'package:multipass_gui/vm_table/vms.dart';

class _FixedSelectedVmsNotifier extends SelectedVmsNotifier {
  final BuiltSet<String> _initial;

  _FixedSelectedVmsNotifier(this._initial);

  @override
  BuiltSet<String> build() => _initial;
}

ProviderScope _wrap(
  Widget child, {
  BuiltSet<String>? selectedVms,
  List<VmInfo>? vmInfos,
  List<String>? vmNames,
}) {
  return ProviderScope(
    overrides: [
      selectedVmsProvider.overrideWith(
        () => _FixedSelectedVmsNotifier(selectedVms ?? BuiltSet()),
      ),
      if (vmInfos != null) vmInfosProvider.overrideWith((ref) => vmInfos),
      if (vmNames != null)
        vmNamesProvider.overrideWith((ref) => vmNames.toBuiltSet()),
    ],
    child: MaterialApp(home: Scaffold(body: child)),
  );
}

void main() {
  group('SelectVmCheckbox', () {
    testWidgets('is unchecked when vm is not in selectedVmsProvider',
        (tester) async {
      await tester.pumpWidget(_wrap(const SelectVmCheckbox('vm1')));

      final checkbox = tester.widget<Checkbox>(find.byType(Checkbox));
      expect(checkbox.value, isFalse);
    });

    testWidgets('is checked when vm is in selectedVmsProvider', (tester) async {
      await tester.pumpWidget(
        _wrap(
          const SelectVmCheckbox('vm1'),
          selectedVms: BuiltSet(['vm1']),
        ),
      );

      final checkbox = tester.widget<Checkbox>(find.byType(Checkbox));
      expect(checkbox.value, isTrue);
    });

    testWidgets('tapping adds the vm to selectedVmsProvider', (tester) async {
      late WidgetRef capturedRef;

      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            selectedVmsProvider.overrideWith(
              () => _FixedSelectedVmsNotifier(BuiltSet()),
            ),
          ],
          child: MaterialApp(
            home: Scaffold(
              body: Consumer(
                builder: (_, ref, __) {
                  capturedRef = ref;
                  return const SelectVmCheckbox('vm1');
                },
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.byType(Checkbox));
      await tester.pump();

      expect(capturedRef.read(selectedVmsProvider), contains('vm1'));
    });
  });

  group('SelectAllCheckbox', () {
    final twoVms = [
      DetailedInfoItem(name: 'vm1'),
      DetailedInfoItem(name: 'vm2'),
    ];

    testWidgets('shows false when no vms are selected', (tester) async {
      await tester.pumpWidget(
        _wrap(const SelectAllCheckbox(), vmInfos: twoVms),
      );

      final checkbox = tester.widget<Checkbox>(find.byType(Checkbox));
      expect(checkbox.value, isFalse);
    });

    testWidgets('shows true when all vms are selected', (tester) async {
      await tester.pumpWidget(
        _wrap(
          const SelectAllCheckbox(),
          vmInfos: twoVms,
          selectedVms: BuiltSet(['vm1', 'vm2']),
        ),
      );

      final checkbox = tester.widget<Checkbox>(find.byType(Checkbox));
      expect(checkbox.value, isTrue);
    });

    testWidgets('shows null (tristate) when only some vms are selected',
        (tester) async {
      await tester.pumpWidget(
        _wrap(
          const SelectAllCheckbox(),
          vmInfos: twoVms,
          selectedVms: BuiltSet(['vm1']),
        ),
      );

      final checkbox = tester.widget<Checkbox>(find.byType(Checkbox));
      expect(checkbox.value, isNull);
    });
  });

  group('VmNameLink', () {
    testWidgets('tapping navigates to the vm details page', (tester) async {
      late WidgetRef capturedRef;

      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            vmNamesProvider.overrideWith((ref) => BuiltSet(['test-vm'])),
          ],
          child: MaterialApp(
            home: Scaffold(
              body: Consumer(
                builder: (_, ref, __) {
                  capturedRef = ref;
                  return const VmNameLink('test-vm');
                },
              ),
            ),
          ),
        ),
      );

      // Ensure the autoDispose sidebarKeyProvider is alive before tapping.
      capturedRef.read(sidebarKeyProvider);

      await tester.tapAt(tester.getCenter(find.byType(VmNameLink)));
      await tester.pump();

      expect(capturedRef.read(sidebarKeyProvider), equals('vm-test-vm'));
    });
  });
}
