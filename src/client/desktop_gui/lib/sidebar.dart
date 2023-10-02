import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:window_manager/window_manager.dart';

import 'catalogue.dart';
import 'providers.dart';
import 'vm_table/vm_table_screen.dart';

final sidebarKeyProvider = StateProvider((_) => CatalogueScreen.sidebarKey);
const sidebarWidgets = {
  CatalogueScreen.sidebarKey: CatalogueScreen(),
  VmTableScreen.sidebarKey: VmTableScreen(),
};

class SideBar extends ConsumerWidget {
  const SideBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedSidebarKey = ref.watch(sidebarKeyProvider);

    Widget buildButton({
      required Widget icon,
      required Widget child,
      String? sidebarKey,
      required void Function() onPressed,
    }) {
      return Container(
        margin: const EdgeInsets.symmetric(vertical: 4),
        decoration: BoxDecoration(
          border: Border(
            left: sidebarKey == selectedSidebarKey
                ? const BorderSide(color: Colors.white, width: 2)
                : BorderSide.none,
          ),
        ),
        child: TextButton.icon(
          icon: icon,
          label: child,
          onPressed: onPressed,
          style: TextButton.styleFrom(
            alignment: Alignment.centerLeft,
            padding: const EdgeInsets.symmetric(vertical: 18, horizontal: 12),
            shape: const LinearBorder(),
            foregroundColor: Colors.white,
            backgroundColor: sidebarKey == selectedSidebarKey
                ? const Color(0xff444444)
                : Colors.transparent,
            textStyle: const TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.w300,
            ),
          ),
        ),
      );
    }

    final catalogue = buildButton(
      icon: SvgPicture.asset('assets/catalogue.svg'),
      sidebarKey: CatalogueScreen.sidebarKey,
      child: const Text('Catalogue'),
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          CatalogueScreen.sidebarKey,
    );

    final instances = buildButton(
      icon: SvgPicture.asset('assets/instances.svg'),
      sidebarKey: VmTableScreen.sidebarKey,
      child: Row(children: [
        const Expanded(child: Text('Instances')),
        Consumer(builder: (_, ref, __) {
          final nVms = ref.watch(vmInfosProvider.select((vms) => vms.length));
          return Text('$nVms');
        }),
      ]),
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          VmTableScreen.sidebarKey,
    );

    final exit = buildButton(
      icon: SvgPicture.asset('assets/exit.svg'),
      child: const Text('Close Application'),
      onPressed: windowManager.destroy,
    );

    final header = Row(crossAxisAlignment: CrossAxisAlignment.end, children: [
      Container(
        alignment: Alignment.bottomCenter,
        color: const Color(0xffE95420),
        height: 50,
        margin: const EdgeInsets.symmetric(horizontal: 8),
        padding: const EdgeInsets.all(4),
        child: SvgPicture.asset('assets/multipass.svg', width: 20),
      ),
      const DefaultTextStyle(
        style: TextStyle(
          color: Colors.white,
          fontWeight: FontWeight.w300,
          height: 1,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Canonical', style: TextStyle(fontSize: 12)),
            Text('Multipass', style: TextStyle(fontSize: 24)),
          ],
        ),
      ),
    ]);

    return Container(
      color: const Color(0xff262626),
      padding: const EdgeInsets.symmetric(horizontal: 8),
      width: 240,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          header,
          const SizedBox(height: 15),
          catalogue,
          instances,
          const Spacer(),
          Divider(color: Colors.white.withOpacity(0.3)),
          exit,
        ],
      ),
    );
  }
}
