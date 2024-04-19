import 'package:basics/basics.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../providers.dart';
import 'terminal.dart';

class Tab extends StatelessWidget {
  final String title;
  final bool selected;
  final VoidCallback onTap;
  final VoidCallback onClose;

  const Tab({
    super.key,
    required this.title,
    required this.selected,
    required this.onTap,
    required this.onClose,
  });

  static final ubuntuIcon = Container(
    alignment: Alignment.center,
    color: const Color(0xffE95420),
    margin: const EdgeInsets.symmetric(horizontal: 10),
    width: 17,
    height: 17,
    child: SvgPicture.asset(
      'assets/ubuntu.svg',
      width: 12,
      colorFilter: const ColorFilter.mode(
        Colors.white,
        BlendMode.srcIn,
      ),
    ),
  );

  @override
  Widget build(BuildContext context) {
    final tabTitle = Text(
      title,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: const TextStyle(
        color: Colors.white,
        fontWeight: FontWeight.w300,
      ),
    );

    final closeButton = Material(
      color: Colors.transparent,
      child: IconButton(
        hoverColor: Colors.white24,
        splashRadius: 10,
        onPressed: onClose,
        icon: const Icon(Icons.close, color: Colors.white, size: 17),
      ),
    );

    final decoration = BoxDecoration(
      color: Color(selected ? 0xff2B2B2B : 0xff222222),
      border: selected
          ? const Border(
              bottom: BorderSide(color: Color(0xffE95420), width: 2.5),
            )
          : null,
    );

    return GestureDetector(
      onTap: onTap,
      child: Container(
        width: 190,
        decoration: decoration,
        child: Row(children: [
          ubuntuIcon,
          Expanded(child: tabTitle),
          closeButton,
        ]),
      ),
    );
  }
}

class TerminalTabs extends ConsumerStatefulWidget {
  final String name;

  const TerminalTabs(this.name, {super.key});

  @override
  ConsumerState<TerminalTabs> createState() => _TerminalTabsState();
}

class _TerminalTabsState extends ConsumerState<TerminalTabs> {
  var _currentIndex = 0;

  int get currentIndex => _currentIndex;

  set currentIndex(int value) {
    final (_, key, __) = shells[value];
    FocusManager.instance.primaryFocus?.unfocus();
    key.currentState?.focusNode.requestFocus();
    _currentIndex = value;
  }

  final shells = [(1, GlobalKey<VmTerminalState>(), false)];

  @override
  Widget build(BuildContext context) {
    final tabs = shells.indexed.map((e) {
      final (orderIndex, (shellId, _, __)) = e;
      return ReorderableDragStartListener(
        key: ValueKey(shellId),
        index: orderIndex,
        child: Tab(
          title: 'Shell $shellId',
          selected: orderIndex == currentIndex,
          onTap: () => setState(() => currentIndex = orderIndex),
          onClose: () {
            setState(() {
              shells.removeAt(orderIndex);
              if (orderIndex < currentIndex) currentIndex -= 1;
              if (shells.isEmpty) shells.add((1, GlobalKey(), false));
              currentIndex = currentIndex.clamp(0, shells.length - 1);
            });
          },
        ),
      );
    });

    final terminals = shells.map((e) {
      final (_, terminalKey, running) = e;
      return VmTerminal(widget.name, key: terminalKey, running: running);
    });

    final addShellButton = Material(
      color: Colors.transparent,
      child: IconButton(
        hoverColor: Colors.white24,
        splashRadius: 10,
        icon: const Icon(Icons.add, color: Colors.white, size: 20),
        onPressed: () => setState(() {
          final shellIds = shells.map((e) => e.$1).toSet();
          final newShellId = 1.to(1000).whereNot(shellIds.contains).first;
          final running = ref.read(vmInfoProvider(widget.name).select((info) {
            return info.instanceStatus.status == Status.RUNNING;
          }));
          shells.add((newShellId, GlobalKey(), running));
          currentIndex = shells.length - 1;
        }),
      ),
    );

    final tabList = ReorderableListView(
      buildDefaultDragHandles: false,
      footer: addShellButton,
      scrollDirection: Axis.horizontal,
      onReorderStart: (index) => setState(() => currentIndex = index),
      onReorder: (oldIndex, newIndex) => setState(() {
        if (oldIndex < newIndex) newIndex -= 1;
        final item = shells.removeAt(oldIndex);
        shells.insert(newIndex, item);
        currentIndex = newIndex;
      }),
      children: tabs.toList(),
    );

    final shellStack = IndexedStack(
      sizing: StackFit.expand,
      index: currentIndex,
      children: terminals.toList(),
    );

    return Column(children: [
      Container(
        color: const Color(0xff222222),
        height: 35,
        child: tabList,
      ),
      Expanded(child: shellStack),
    ]);
  }
}
