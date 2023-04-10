import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import 'grpc_client.dart';
import 'help.dart';
import 'vms_screen.dart';

final sidebarKeyProvider = StateProvider((_) => VMsScreen.sidebarKey);
final sidebarWidgets = {
  VMsScreen.sidebarKey: const VMsScreen(),
  HelpScreen.sidebarKey: const HelpScreen(),
};

class SideBar extends ConsumerWidget {
  const SideBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedSidebarKey = ref.watch(sidebarKeyProvider);
    final nInstances = ref.watch(
      vmInfosProvider.select((infos) => infos.length),
    );

    FilledButton buildFilledButton({
      required String sidebarKey,
      required Widget icon,
      required Widget child,
    }) {
      return FilledButton.icon(
        icon: icon,
        label: child,
        onPressed: () =>
            ref.read(sidebarKeyProvider.notifier).state = sidebarKey,
        style: ButtonStyle(
          overlayColor: const MaterialStatePropertyAll(Colors.white24),
          padding: const MaterialStatePropertyAll(
            EdgeInsets.symmetric(vertical: 18, horizontal: 12),
          ),
          backgroundColor: MaterialStatePropertyAll(
            sidebarKey == selectedSidebarKey
                ? Colors.white.withOpacity(0.15)
                : Colors.transparent,
          ),
        ),
      );
    }

    final instancesTab = buildFilledButton(
      sidebarKey: VMsScreen.sidebarKey,
      icon: SvgPicture.asset('assets/instance.svg'),
      child: Row(mainAxisAlignment: MainAxisAlignment.spaceBetween, children: [
        const Text('Instances'),
        Text('$nInstances'),
      ]),
    );

    final helpTab = buildFilledButton(
      sidebarKey: HelpScreen.sidebarKey,
      icon: const Icon(Icons.help_outline),
      child: const Align(alignment: Alignment.centerLeft, child: Text('Help')),
    );

    return Container(
      color: const Color(0xff181818),
      padding: const EdgeInsets.all(14),
      width: 250,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          instancesTab,
          const Spacer(),
          helpTab,
        ],
      ),
    );
  }
}
