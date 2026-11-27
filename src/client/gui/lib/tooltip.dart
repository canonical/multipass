import 'package:material_ui/material_ui.dart' as fl;

import 'colors.dart';

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
        key: fl.Key(message),
        message: message,
        textAlign: fl.TextAlign.center,
        decoration: fl.BoxDecoration(
          color: context.colors.tooltipBackground,
          borderRadius: fl.BorderRadius.circular(2),
        ),
        child: child,
      ),
    );
  }
}
