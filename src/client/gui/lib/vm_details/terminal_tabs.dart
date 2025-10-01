import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:fpdart/fpdart.dart';

import '../close_terminal_dialog.dart';
import '../providers.dart';
import 'terminal.dart';

typedef ShellIds = ({
  BuiltList<ShellId> ids,
  // this is the index of the currently selected shell id
  int currentIndex,
});

class ShellIdsNotifier extends Notifier<ShellIds> {
  ShellIdsNotifier(this.arg);
  final String arg;

  @override
  ShellIds build() => (ids: [ShellId(1)].build(), currentIndex: 0);

  void add() {
    final ids = state.ids;
    final existingIds = state.ids.map((shellId) => shellId.id).toBuiltSet();
    final newShellId = 1.to(1000).whereNot(existingIds.contains).first;
    state = (
      ids: ids.rebuild((ids) => ids.add(ShellId(newShellId))),
      currentIndex: ids.length,
    );
  }

  void remove(int index) {
    var (:ids, :currentIndex) = state;
    final idsBuilder = ids.toBuilder();
    idsBuilder.removeAt(index);
    if (index < currentIndex) currentIndex -= 1;
    if (idsBuilder.isEmpty) idsBuilder.add(ShellId(1, autostart: false));
    currentIndex = currentIndex.clamp(0, idsBuilder.length - 1);
    state = (ids: idsBuilder.build(), currentIndex: currentIndex);
  }

  void reorder(int oldIndex, int newIndex) {
    if (oldIndex < newIndex) newIndex -= 1;
    final reorderedIds = state.ids.rebuild((ids) {
      final id = ids.removeAt(oldIndex);
      ids.insert(newIndex, id);
    });

    state = (ids: reorderedIds, currentIndex: newIndex);
  }

  void setCurrent(int index) {
    state = (ids: state.ids, currentIndex: index);
  }
}

final shellIdsProvider = NotifierProvider.autoDispose
    .family<ShellIdsNotifier, ShellIds, String>(ShellIdsNotifier.new);

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
      colorFilter: const ColorFilter.mode(Colors.white, BlendMode.srcIn),
    ),
  );

  @override
  Widget build(BuildContext context) {
    final tabTitle = Text(
      title,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: const TextStyle(color: Colors.white, fontWeight: FontWeight.w300),
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
        child: Row(
          children: [
            ubuntuIcon,
            Expanded(child: tabTitle),
            closeButton,
          ],
        ),
      ),
    );
  }
}

class TerminalTabs extends ConsumerWidget {
  final String name;

  const TerminalTabs(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final provider = shellIdsProvider(name);
    final notifier = provider.notifier;
    final (:ids, :currentIndex) = ref.watch(provider);
    final askTerminalCloseProvider = guiSettingProvider(askTerminalCloseKey);

    final tabsAndShells = ids.mapIndexed((index, shellId) {
      final tab = ReorderableDragStartListener(
        key: ValueKey(shellId.id),
        index: index,
        child: Tab(
          title: 'Shell ${shellId.id}',
          selected: index == currentIndex,
          onTap: () => ref.read(notifier).setCurrent(index),
          onClose: () {
            final ask = ref.read(
              askTerminalCloseProvider.select((ask) {
                return ask?.toBoolOption.toNullable() ?? true;
              }),
            );
            final terminalKey = (vmName: name, shellId: shellId);
            if (!ask || ref.read(terminalProvider(terminalKey)) == null) {
              ref.read(notifier).remove(index);
              return;
            }
            showDialog(
              context: context,
              barrierDismissible: false,
              builder: (context) {
                return CloseTerminalDialog(
                  onYes: () {
                    Navigator.pop(context);
                    ref.read(notifier).remove(index);
                  },
                  onNo: () => Navigator.pop(context),
                  onDoNotAsk: (doNotAsk) => ref
                      .read(askTerminalCloseProvider.notifier)
                      .set('${!doNotAsk}'),
                );
              },
            );
          },
        ),
      );

      final shell = VmTerminal(
        key: GlobalObjectKey(shellId),
        name,
        shellId,
        isCurrent: index == currentIndex,
      );

      return (tab: tab, shell: shell);
    }).toList();

    final addShellButton = Material(
      color: Colors.transparent,
      child: IconButton(
        hoverColor: Colors.white24,
        splashRadius: 10,
        icon: const Icon(Icons.add, color: Colors.white, size: 20),
        onPressed: () => ref.read(notifier).add(),
      ),
    );

    final tabList = ReorderableListView(
      buildDefaultDragHandles: false,
      footer: addShellButton,
      scrollDirection: Axis.horizontal,
      onReorderStart: (index) => ref.read(notifier).setCurrent(index),
      onReorder: (oldIndex, newIndex) {
        ref.read(notifier).reorder(oldIndex, newIndex);
      },
      children: tabsAndShells.map((e) => e.tab).toList(),
    );

    final shellStack = FocusScope(
      child: IndexedStack(
        sizing: StackFit.expand,
        index: currentIndex,
        children: tabsAndShells.map((e) => e.shell).toList(),
      ),
    );

    return Column(
      children: [
        Container(color: const Color(0xff222222), height: 35, child: tabList),
        Expanded(child: shellStack),
      ],
    );
  }
}
