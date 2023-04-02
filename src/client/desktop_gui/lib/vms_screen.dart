import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import 'bulk_actions_bar.dart';
import 'grpc_client.dart';
import 'help.dart';
import 'launch_panel.dart';
import 'running_only_switch.dart';
import 'sidebar.dart';
import 'text_span_ext.dart';
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

final vmScreenScaffold = GlobalKey<ScaffoldMessengerState>();

final documentationUrl = Uri.parse(
  'https://multipass.run/docs/use-an-instance',
);

class VMsScreen extends StatelessWidget {
  static const sidebarKey = 'instances';

  const VMsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final textWithLink = Consumer(builder: (_, ref, __) {
      final hasInstances = ref.watch(
        vmInfosProvider.select((infos) => infos.isNotEmpty),
      );

      return hasInstances
          ? RichText(
              text: [
                'This table shows the list of instances or virtual machines'
                        ' created and managed by Multipass. '
                    .span,
                'Learn more about instances.'
                    .span
                    .link(() => launchUrl(documentationUrl), ref),
              ].spans,
            )
          : RichText(
              text: [
                'Launch a new instance to start. Read more about how to '.span,
                'get started on Multipass.'.span.link(
                    () => ref.read(sidebarKeyProvider.notifier).state =
                        HelpScreen.sidebarKey,
                    ref),
              ].spans,
            );
    });

    final searchBox = SizedBox(
      width: 385,
      height: 40,
      child: Consumer(
        builder: (_, ref, __) => TextField(
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
          onChanged: (name) =>
              ref.read(searchNameProvider.notifier).state = name,
        ),
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

    var launchButton = Builder(builder: (context) {
      return ElevatedButton.icon(
        icon: const Icon(Icons.add),
        label: const Text('Launch new instance'),
        onPressed: () => Scaffold.of(context).openEndDrawer(),
        style: const ButtonStyle(
          iconColor: MaterialStatePropertyAll(Colors.white),
          foregroundColor: MaterialStatePropertyAll(Colors.white),
          backgroundColor: MaterialStatePropertyAll(Color(0xff006ada)),
        ),
      );
    });

    final vmsWithActionBar = Container(
      clipBehavior: Clip.antiAlias,
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(8),
        border: Border.all(
          width: 0.5,
          color: Colors.black54,
        ),
      ),
      child: Column(
        children: [
          Container(
            padding: const EdgeInsets.all(8).copyWith(left: 4),
            decoration: const BoxDecoration(
              color: Color(0xff313033),
              boxShadow: [BoxShadow(blurRadius: 5, color: Colors.black54)],
            ),
            child: Row(
              children: [
                const BulkActionsBar(),
                const Spacer(),
                launchButton,
              ],
            ),
          ),
          const Expanded(child: VMsTable()),
        ],
      ),
    );

    return ScaffoldMessenger(
      key: vmScreenScaffold,
      child: Scaffold(
        drawerScrimColor: Colors.transparent,
        endDrawer: Drawer(
          surfaceTintColor: Colors.transparent,
          width: 420,
          elevation: 5,
          shadowColor: Colors.black,
          shape: const Border(),
          child: LaunchPanel(),
        ),
        body: Padding(
          padding: const EdgeInsets.all(15),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
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
              Expanded(child: vmsWithActionBar),
            ],
          ),
        ),
      ),
    );
  }
}
