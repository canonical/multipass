import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'bulk_actions_bar.dart';
import 'globals.dart';
import 'grpc_client.dart';
import 'running_only_switch.dart';
import 'vms_table.dart';

final runningOnlyProvider = StateProvider((_) => false);
final searchNameProvider = StateProvider((_) => '');
final selectedVMsProvider = StateProvider<Map<String, Status>>((ref) {
  ref.watch(runningOnlyProvider);
  ref.watch(searchNameProvider);
  ref.listen(vmStatusesProvider, (_, next) {
    ref.controller.update(
      (state) => Map.fromEntries(
        next.entries.where((e) => state.containsKey(e.key)),
      ),
    );
  });

  return {};
});

class VMsScreen extends ConsumerWidget {
  const VMsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
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
        final n = ref.watch(vmInfosProvider.select((infos) => infos.length));
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
