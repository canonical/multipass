import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:window_manager/window_manager.dart';

import 'catalogue/catalogue.dart';
import 'providers.dart';
import 'vm_table/vm_table_screen.dart';

final sidebarKeyProvider = StateProvider((_) => CatalogueScreen.sidebarKey);
final sidebarWidgetProvider =
    Provider<Widget>((ref) => sidebarWidgets[ref.watch(sidebarKeyProvider)]!);
const sidebarWidgets = {
  CatalogueScreen.sidebarKey: CatalogueScreen(),
  VmTableScreen.sidebarKey: VmTableScreen(),
};

class SideBar extends ConsumerWidget {
  const SideBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedSidebarKey = ref.watch(sidebarKeyProvider);
    final vmNames = ref.watch(vmNamesProvider);

    bool isSelected(String key) => key == selectedSidebarKey;

    final catalogue = SidebarEntry(
      icon: SvgPicture.asset('assets/catalogue.svg'),
      selected: isSelected(CatalogueScreen.sidebarKey),
      child: const Text('Catalogue'),
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          CatalogueScreen.sidebarKey,
    );

    final instances = SidebarEntry(
      icon: SvgPicture.asset('assets/instances.svg'),
      selected: isSelected(VmTableScreen.sidebarKey),
      child: Row(children: [
        const Expanded(child: Text('Instances')),
        Text('${vmNames.length}'),
      ]),
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          VmTableScreen.sidebarKey,
    );

    final exit = SidebarEntry(
      icon: SvgPicture.asset('assets/exit.svg'),
      onPressed: windowManager.destroy,
      child: const Text('Close Application'),
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

    final instanceEntries = vmNames.map((name) {
      final key = 'instance-$name';
      return SidebarEntry(
        icon: const SizedBox(width: 30),
        selected: isSelected(key),
        child: Text(name),
        onPressed: () {},
      );
    });

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
          ...instanceEntries,
          const Spacer(),
          Divider(color: Colors.white.withOpacity(0.3)),
          exit,
        ],
      ),
    );
  }
}

class SidebarEntry extends StatelessWidget {
  final Widget child;
  final Widget icon;
  final VoidCallback onPressed;
  final bool selected;

  const SidebarEntry({
    super.key,
    required this.child,
    required this.icon,
    required this.onPressed,
    this.selected = false,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 4),
      decoration: BoxDecoration(
        border: Border(
          left: selected
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
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(vertical: 18, horizontal: 12),
          shape: const LinearBorder(),
          textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.w300),
          backgroundColor:
              selected ? const Color(0xff444444) : Colors.transparent,
        ),
      ),
    );
  }
}
