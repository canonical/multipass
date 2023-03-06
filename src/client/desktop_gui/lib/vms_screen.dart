import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'bulk_actions_bar.dart';
import 'generated/multipass.pbgrpc.dart';
import 'globals.dart';
import 'running_only_switch.dart';
import 'vms_table.dart';

final runningOnlyProvider = StateProvider((_) => false);
final searchNameProvider = StateProvider((_) => '');
final selectedVMsProvider = StateProvider((ref) {
  ref.watch(runningOnlyProvider);
  ref.watch(searchNameProvider);
  return <String, InfoReply_Info>{};
});

MapEntry<String, Status> infoToStatusMap(String key, InfoReply_Info value) {
  return MapEntry(key, value.instanceStatus.status);
}

class VMsScreen extends ConsumerWidget {
  const VMsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    // this updates the map of selected VMs
    ref.listen(infoStreamProvider.select((reply) {
      final infos = reply.valueOrNull?.info ?? [];
      return {for (final info in infos) info.name: info};
    }), (previous, next) {
      if (!mapEquals(
        previous?.map(infoToStatusMap),
        next.map(infoToStatusMap),
      )) {
        ref.read(selectedVMsProvider.notifier).update(
              (state) => Map.fromEntries(
                next.entries.where((e) => state.containsKey(e.key)),
              ),
            );
      }
    });

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

    final searchBox = SizedBox(
      width: 385,
      height: 40,
      child: TextField(
        decoration: InputDecoration(
          hintText: 'Search by name',
          fillColor: const Color(0xffe3dee2),
          filled: true,
          suffixIcon: const Icon(Icons.search, color: Colors.black),
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(50),
            borderSide: BorderSide.none,
          ),
        ),
        textAlignVertical: TextAlignVertical.bottom,
        onChanged: (name) => ref.read(searchNameProvider.notifier).state = name,
      ),
    );

    var numberOfInstances = Consumer(
      builder: (_, ref, __) {
        final n = ref.watch(infoStreamProvider
            .select((reply) => reply.valueOrNull?.info.length ?? 0));
        return Text(
          'Instance ($n)',
          style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w700),
        );
      },
    );

    return Padding(
      padding: const EdgeInsets.all(15),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        numberOfInstances,
        const Divider(),
        textWithLink,
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 15),
          child: Row(children: [
            const RunningOnlySwitch(),
            const Spacer(),
            searchBox,
          ]),
        ),
        const BulkActionsBar(),
        const Expanded(child: VMsTable()),
      ]),
    );
  }
}
