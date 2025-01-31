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
  bool _copied = false;
  bool get _isCopyable => widget.text != '-';

  void _copyToClipboard() async {
    if (!_isCopyable) return;
    await Clipboard.setData(ClipboardData(text: widget.text));
    setState(() => _copied = true);
  }

  void _resetCopied() {
    if (_copied) {
      setState(() => _copied = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    Widget text = Text(
      widget.text,
      style: widget.style,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
    );

    if (!_isCopyable) return text;

    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onExit: (_) => _resetCopied(),
      child: GestureDetector(
        onTap: _copyToClipboard,
        child: Tooltip(
          message: _copied ? 'Copied' : 'Click to copy',
          child: text,
        ),
      ),
    );
  }
}
