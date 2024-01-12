import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:window_manager/window_manager.dart';

import 'catalogue/catalogue.dart';
import 'help.dart';
import 'providers.dart';
import 'settings/settings.dart';
import 'vm_table/vm_table_screen.dart';

final sidebarKeyProvider = StateProvider((_) => CatalogueScreen.sidebarKey);
final sidebarWidgetProvider =
    Provider<Widget>((ref) => sidebarWidgets[ref.watch(sidebarKeyProvider)]!);
const sidebarWidgets = {
  CatalogueScreen.sidebarKey: CatalogueScreen(),
  VmTableScreen.sidebarKey: VmTableScreen(),
  SettingsScreen.sidebarKey: SettingsScreen(),
  HelpScreen.sidebarKey: HelpScreen(),
};

final sidebarExpandedProvider = StateProvider((_) => false);

class SideBar extends ConsumerWidget {
  static const animationDuration = Duration(milliseconds: 200);


  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedSidebarKey = ref.watch(sidebarKeyProvider);
    final vmNames = ref.watch(vmNamesProvider);

    bool isSelected(String key) => key == selectedSidebarKey;

    final catalogue = SidebarEntry(
      icon: SvgPicture.asset('assets/catalogue.svg'),
      selected: isSelected(CatalogueScreen.sidebarKey),
      label: 'Catalogue',
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          CatalogueScreen.sidebarKey,
    );

    final instances = SidebarEntry(
      icon: SvgPicture.asset('assets/instances.svg'),
      selected: isSelected(VmTableScreen.sidebarKey),
      label: 'Instances',
      badge: vmNames.length.toString(),
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          VmTableScreen.sidebarKey,
    );

    final help = SidebarEntry(
      icon: SvgPicture.asset('assets/help.svg'),
      selected: isSelected(HelpScreen.sidebarKey),
      label: 'Help',
      onPressed: () =>
          ref.read(sidebarKeyProvider.notifier).state = HelpScreen.sidebarKey,
    );

    final settings = SidebarEntry(
      icon: SvgPicture.asset('assets/settings.svg'),
      selected: isSelected(SettingsScreen.sidebarKey),
      label: 'Settings',
      onPressed: () => ref.read(sidebarKeyProvider.notifier).state =
          SettingsScreen.sidebarKey,
    );

    final exit = SidebarEntry(
      icon: SvgPicture.asset('assets/exit.svg'),
      onPressed: windowManager.destroy,
      label: 'Close Application',
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
        icon: Flexible(
          child: Container(
            constraints: const BoxConstraints(maxWidth: 30),
          ),
        ),
        selected: isSelected(key),
        label: name,
        onPressed: () {},
      );
    });


class SidebarEntry extends ConsumerWidget {
  final String label;
  final Widget icon;
  final String? badge;
  final VoidCallback onPressed;
  final bool selected;

  const SidebarEntry({
    super.key,
    required this.label,
    required this.icon,
    this.badge,
    required this.onPressed,
    this.selected = false,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final expanded = ref.watch(sidebarExpandedProvider);

    return Container(
      margin: const EdgeInsets.symmetric(vertical: 4),
      decoration: BoxDecoration(
        border: Border(
          left: selected
              ? const BorderSide(color: Colors.white, width: 2)
              : BorderSide.none,
        ),
      ),
      child: TextButton(
        onPressed: onPressed,
        style: TextButton.styleFrom(
          padding: const EdgeInsets.symmetric(vertical: 18, horizontal: 12),
          backgroundColor:
              selected ? const Color(0xff444444) : Colors.transparent,
        ),
        child: Row(children: [
          badge == null
              ? icon
              : Badge(
                  backgroundColor: const Color(0xff333333),
                  isLabelVisible: !expanded,
                  label: Text(badge!),
                  offset: const Offset(10, -6),
                  child: icon,
                ),
          Expanded(
            flex: 5,
            child: AnimatedOpacity(
              duration: SideBar.animationDuration,
              opacity: expanded ? 1 : 0,
              child: Text('    $label', softWrap: false),
            ),
          ),
          if (badge != null && expanded)
            Flexible(
              child: Container(
                decoration: const BoxDecoration(
                  color: Color(0xff333333),
                  shape: BoxShape.circle,
                ),
                alignment: Alignment.center,
                child: Text(badge!, softWrap: false),
              ),
            ),
        ]),
      ),
    );
  }
}
