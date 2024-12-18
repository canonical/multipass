import 'package:flutter/material.dart';

class WhiteSelectableText extends StatelessWidget {
  final String text;
  final TextStyle? style;
  final TextAlign? textAlign;
  final int? maxLines;

  const WhiteSelectableText(
    this.text, {
    super.key,
    this.style,
    this.textAlign,
    this.maxLines,
  });

  @override
  Widget build(BuildContext context) {
    final textButtonStyle = Theme.of(context).textButtonTheme.style?.copyWith(
      backgroundColor: const MaterialStatePropertyAll(Colors.white),
    );

    return SelectableText(
      text,
      style: style?.copyWith(overflow: TextOverflow.ellipsis) ?? const TextStyle(overflow: TextOverflow.ellipsis),
      textAlign: textAlign,
      maxLines: maxLines,
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
    );
  }
}
