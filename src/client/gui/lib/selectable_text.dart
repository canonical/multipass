import 'package:flutter/material.dart' as fl;

class SelectableText extends fl.StatelessWidget {
  final String text;
  final fl.TextStyle? style;
  final fl.TextAlign? textAlign;
  final int? maxLines;

  const SelectableText(
      this.text, {
        super.key,
        this.style,
        this.textAlign,
        this.maxLines,
      });

  @override
  fl.Widget build(fl.BuildContext context) {
    final textButtonStyle = fl.Theme.of(context).textButtonTheme.style?.copyWith(
      backgroundColor: const fl.WidgetStatePropertyAll(fl.Colors.transparent),
    );

    return fl.SelectableText(
      text,
      style: style?.copyWith(overflow: fl.TextOverflow.ellipsis) ??
          const fl.TextStyle(overflow: fl.TextOverflow.ellipsis),
      textAlign: textAlign,
      maxLines: maxLines,
      contextMenuBuilder: (context, editableTextState) {
        return fl.TapRegion(
          onTapOutside: (_) => fl.ContextMenuController.removeAny(),
          child: fl.TextButtonTheme(
            data: fl.TextButtonThemeData(style: textButtonStyle),
            child: fl.AdaptiveTextSelectionToolbar.buttonItems(
              anchors: editableTextState.contextMenuAnchors,
              buttonItems: editableTextState.contextMenuButtonItems,
            ),
          ),
        );
      },
    );
  }
}
