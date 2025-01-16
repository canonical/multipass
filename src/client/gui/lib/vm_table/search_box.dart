import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final searchNameProvider = StateProvider((_) => '');

class SearchBox extends ConsumerWidget {
  const SearchBox({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final textButtonStyle = Theme.of(context).textButtonTheme.style?.copyWith(
          backgroundColor: const MaterialStatePropertyAll(Colors.transparent),
        );

    return SizedBox(
      width: 220,
      child: TextField(
        decoration: const InputDecoration(
          hintText: 'Search instances...',
          suffixIcon: Icon(Icons.search),
        ),
        onChanged: (name) => ref.read(searchNameProvider.notifier).state = name,
        contextMenuBuilder: (context, editableTextState) {
          return TapRegion(
            onTapOutside: (_) => ContextMenuController.removeAny(),
            child: TextButtonTheme(
              data: TextButtonThemeData(style: textButtonStyle),
              child: AdaptiveTextSelectionToolbar.buttonItems(
                anchors: editableTextState.contextMenuAnchors,
                buttonItems: editableTextState.contextMenuButtonItems,
              ),
            ),
          );
        },
      ),
    );
  }
}
