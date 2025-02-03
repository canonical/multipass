import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter/services.dart';

import 'tooltip.dart';

class CopyableText extends StatefulWidget {
  final String text;
  final TextStyle? style;

  const CopyableText(this.text, {super.key, this.style});

  @override
  State<CopyableText> createState() => _CopyableTextState();
}

class _CopyableTextState extends State<CopyableText> {
  var _copied = false;

  @override
  Widget build(BuildContext context) {
    Widget text = Text(
      widget.text,
      style: widget.style,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
    );

    // if there is no value, display "-"
    if (widget.text == '-') return text;

    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onExit: (_) => setState(() => _copied = false),
      child: GestureDetector(
        onTap: () async {
          await Clipboard.setData(ClipboardData(text: widget.text));
          setState(() => _copied = true);
        },
        child: Tooltip(
          message: _copied ? 'Copied' : 'Click to copy',
          child: text,
        ),
      ),
    );
  }
}
