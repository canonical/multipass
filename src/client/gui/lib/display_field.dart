import 'package:flutter/material.dart';

import 'copyable_text.dart';

class DisplayField extends StatelessWidget {
  final String? label;
  final TextStyle labelStyle;
  final String? text;
  final double width;
  final bool copyable;

  const DisplayField({
    super.key,
    this.label,
    this.labelStyle = const TextStyle(fontSize: 16),
    this.text,
    this.width = 360,
    this.copyable = false,
  });

  @override
  Widget build(BuildContext context) {
    // Only build the right-hand side if text is non-null
    final Widget? styledField = text != null
        ? SizedBox(
            width: width,
            child: copyable
                ? CopyableText(
                    text!,
                    style: const TextStyle(fontSize: 16),
                  )
                : Text(
                    text!,
                    style: const TextStyle(fontSize: 16),
                  ),
          )
        : null;

    return Row(
      children: [
        if (label != null)
          Expanded(
            child: Text(
              label!,
              style: labelStyle,
            ),
          ),
        if (styledField != null) styledField,
      ],
    );
  }
}
