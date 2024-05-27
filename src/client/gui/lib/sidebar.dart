import 'dart:async';

import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import 'catalogue/catalogue.dart';
import 'extensions.dart';
import 'help.dart';
import 'providers.dart';
import 'settings/settings.dart';
import 'vm_details/terminal.dart';
import 'vm_table/vm_table_screen.dart';

extension on String {
  String? get sidebarVmName => startsWith('vm-') ? withoutPrefix('vm-') : null;
}

class SidebarKeyNotifier extends Notifier<String> {
  @override
  String build() {
    ref.listen(vmNamesProvider, (_, names) {
      final vmName = state.sidebarVmName;
      if (vmName != null && !names.contains(vmName)) ref.invalidateSelf();
    });

    return CatalogueScreen.sidebarKey;
  }

  void set(String key) {
    if (key.sidebarVmName != null) {
      ref.read(vmVisitedProvider(key).notifier).state = true;
    }
    state = key;
  }
}

final sidebarKeyProvider = NotifierProvider<SidebarKeyNotifier, String>(
  SidebarKeyNotifier.new,
);

final vmVisitedProvider = StateProvider.family<bool, String>((_, __) => false);
final sidebarExpandedProvider = StateProvider((_) => false);
final sidebarPushContentProvider = StateProvider((_) => false);
Timer? sidebarExpandTimer;

class SideBar extends ConsumerWidget {
  static const animationDuration = Duration(milliseconds: 200);

  static const collapsedWidth = 60.0;
  static const expandedWidth = 240.0;

  const SideBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedSidebarKey = ref.watch(sidebarKeyProvider);
    final sidebarKeyNotifier = sidebarKeyProvider.notifier;
    final vmNames = ref.watch(vmNamesProvider);
    final expanded = ref.watch(sidebarExpandedProvider);
    final pushContent = ref.watch(sidebarPushContentProvider);

    bool isSelected(String key) => key == selectedSidebarKey;

    final catalogue = SidebarEntry(
      icon: SvgPicture.asset('assets/catalogue.svg'),
      selected: isSelected(CatalogueScreen.sidebarKey),
      label: 'Catalogue',
      onPressed: () {
        ref.read(sidebarKeyNotifier).set(CatalogueScreen.sidebarKey);
      },
    );

    final instances = SidebarEntry(
      icon: SvgPicture.asset('assets/instances.svg'),
      selected: isSelected(VmTableScreen.sidebarKey) ||
          !expanded && selectedSidebarKey.startsWith('vm-'),
      label: 'Instances',
      badge: vmNames.length.toString(),
      onPressed: () {
        ref.read(sidebarKeyProvider.notifier).set(VmTableScreen.sidebarKey);
      },
    );

    final help = SidebarEntry(
      icon: SvgPicture.asset('assets/help.svg'),
      selected: isSelected(HelpScreen.sidebarKey),
      label: 'Help',
      onPressed: () {
        ref.read(sidebarKeyNotifier).set(HelpScreen.sidebarKey);
      },
    );

    final settings = SidebarEntry(
      icon: SvgPicture.asset('assets/settings.svg'),
      selected: isSelected(SettingsScreen.sidebarKey),
      label: 'Settings',
      onPressed: () {
        ref.read(sidebarKeyNotifier).set(SettingsScreen.sidebarKey);
      },
    );

    final pinSidebarButton = Material(
      color: Colors.transparent,
      child: IconButton(
        hoverColor: Colors.white24,
        splashRadius: 15,
        icon: Icon(
          pushContent ? Icons.chevron_left : Icons.chevron_right,
          color: Colors.white,
        ),
        onPressed: () => ref
            .read(sidebarPushContentProvider.notifier)
            .update((state) => !state),
      ),
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
      Expanded(
        flex: 3,
        child: AnimatedOpacity(
          opacity: expanded ? 1 : 0,
          duration: SideBar.animationDuration,
          child: Text.rich([
            'Canonical\n'.span.size(12).color(Colors.white),
            'Multipass'.span.size(24).color(Colors.white),
          ].spans),
        ),
      ),
      Flexible(child: pinSidebarButton),
    ]);

    final vmEntries = vmNames.map((name) {
      final key = 'vm-$name';
      final hasShells =
          ref.watch(runningShellsProvider(name).select((n) => n > 0));
      return SidebarEntry(
        key: ValueKey(key),
        icon: Opacity(
          opacity: hasShells ? 1 : 0,
          child: SvgPicture.asset(
            'assets/shell.svg',
            width: 15,
            height: 15,
          ),
        ),
        selected: isSelected(key) && expanded,
        label: name,
        onPressed: () {
          ref.read(sidebarKeyNotifier).set(key);
        },
      );
    });

    final sidebar = MouseRegion(
      onEnter: (_) {
        if (pushContent) return;
        sidebarExpandTimer?.cancel();
        sidebarExpandTimer = Timer(const Duration(milliseconds: 200), () {
          ref.read(sidebarExpandedProvider.notifier).state = true;
        });
      },
      onExit: (_) {
        if (pushContent) return;
        sidebarExpandTimer?.cancel();
        ref.read(sidebarExpandedProvider.notifier).state = false;
      },
      child: AnimatedContainer(
        duration: SideBar.animationDuration,
        color: const Color(0xff262626),
        padding: const EdgeInsets.symmetric(horizontal: 8),
        width: expanded ? expandedWidth : collapsedWidth,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            header,
            const SizedBox(height: 15),
            catalogue,
            instances,
            Expanded(child: ListView(children: vmEntries.toList())),
            Divider(color: Colors.white.withOpacity(0.3)),
            help,
            settings,
          ],
        ),
      ),
    );

    return DefaultTextStyle(
      softWrap: false,
      style: const TextStyle(
        height: 1,
        overflow: TextOverflow.clip,
        color: Colors.white,
        fontWeight: FontWeight.w300,
      ),
      child: sidebar,
    );
  }
}

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
