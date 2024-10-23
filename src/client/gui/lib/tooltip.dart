import 'package:flutter/material.dart' as fl;

class Tooltip extends fl.StatelessWidget {
  final fl.Widget child;
  final String message;
  final bool visible;

  const Tooltip({
    super.key,
    required this.child,
    required this.message,
    this.visible = true,
  });

  @override
  fl.Widget build(fl.BuildContext context) {
    return fl.TooltipVisibility(
      visible: visible,
      child: fl.Tooltip(
        message: message,
        textAlign: fl.TextAlign.center,
        decoration: fl.BoxDecoration(
          color: const fl.Color(0xff111111),
          borderRadius: fl.BorderRadius.circular(2),
        ),
        child: child,
      ),
    );
  }
}
