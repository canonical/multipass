import 'package:collection/collection.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'bulk_actions_bar.dart';
import 'generated/multipass.pbgrpc.dart';
import 'globals.dart';
import 'vms_table.dart';

final runningOnlyProvider = StateProvider((_) => false);
final searchNameProvider = StateProvider((_) => '');
final selectedVMsProvider = StateProvider((ref) {
  ref.watch(runningOnlyProvider);
  ref.watch(searchNameProvider);
  return <String, InfoReply_Info>{};
});

class VMsScreen extends ConsumerWidget {
  const VMsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final runningOnly = ref.watch(runningOnlyProvider);
    final searchName = ref.watch(searchNameProvider);
    final selectedVMs = ref.watch(selectedVMsProvider);
    final infos = ref.watch(infoStreamProvider).when(
        data: (reply) => reply.info,
        loading: () => null,
        error: (error, stackTrace) {
          print(error);
          print(stackTrace);
          return ref.read(infoStreamProvider).valueOrNull?.info ?? [];
        });

    ref.listen(infoStreamProvider.select((reply) {
      return reply.valueOrNull?.info.map((e) => e.name) ?? [];
    }), (previous, next) {
      if (!const IterableEquality().equals(previous, next)) {
        ref.read(selectedVMsProvider.notifier).update((state) => {
              for (final name in next)
                if (state.containsKey(name)) name: state[name]!
            });
      }
    });

    final filteredInfos = infos
        ?.where((info) => info.name.contains(searchName))
        .where((info) =>
            !runningOnly || info.instanceStatus.status == Status.RUNNING);

    final textWithLink = RichText(
      text: TextSpan(children: [
        const TextSpan(
          style: TextStyle(color: Colors.black),
          text: 'This table shows the list of instances or virtual machines'
              ' created and managed by Multipass. ',
        ),
        TextSpan(
            style: const TextStyle(color: Colors.blue),
            text: 'Learn more about instances.',
            recognizer: TapGestureRecognizer()..onTap = () {})
      ]),
    );

    final onlyRunningSwitch = SizedBox(
      height: 30,
      child: FittedBox(
        child: Switch(
          value: runningOnly,
          onChanged: infos?.isEmpty ?? true
              ? null
              : (value) => ref.read(runningOnlyProvider.notifier).state = value,
        ),
      ),
    );

    final searchBoxStyle = InputDecoration(
      hintText: 'Search by name',
      fillColor: const Color(0xffe3dee2),
      filled: true,
      suffixIcon: const Icon(Icons.search, color: Colors.black),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(50),
        borderSide: BorderSide.none,
      ),
    );
    final searchBox = SizedBox(
      width: 385,
      height: 40,
      child: TextField(
        decoration: searchBoxStyle,
        textAlignVertical: TextAlignVertical.bottom,
        onChanged: (name) => ref.read(searchNameProvider.notifier).state = name,
      ),
    );

    return Padding(
      padding: const EdgeInsets.all(15),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(
          'Instance (${infos?.length ?? 0})',
          style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w700),
        ),
        const Divider(),
        textWithLink,
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 15),
          child: Row(children: [
            onlyRunningSwitch,
            const Text('Only show running instances.'),
            const Spacer(),
            searchBox,
          ]),
        ),
        const BulkActionsBar(),
        Expanded(
          child: VMsTable(
            infos: filteredInfos,
            selected: selectedVMs,
            onSelect: (newNames) =>
                ref.read(selectedVMsProvider.notifier).state = newNames,
          ),
        ),
      ]),
    );
  }
}
