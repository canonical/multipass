import 'package:flutter/material.dart' as fl;

class Tooltip extends fl.StatefulWidget {
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
  fl.State<Tooltip> createState() => _TooltipState();
}

class _TooltipState extends fl.State<Tooltip> {
  final _key = fl.GlobalKey<fl.TooltipState>();
  bool _forceShow = false;

  @override
  void didUpdateWidget(Tooltip oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.message != widget.message) {
      setState(() => _forceShow = true);
      Future.delayed(const Duration(milliseconds: 1), () {
        if (mounted) {
          setState(() => _forceShow = false);
        }
      });
    }
  }

  @override
  fl.Widget build(fl.BuildContext context) {
    return fl.TooltipVisibility(
      visible: widget.visible,
      child: _forceShow
          ? fl.Tooltip(
              key: _key,
              message: widget.message,
              textAlign: fl.TextAlign.center,
              decoration: fl.BoxDecoration(
                color: const fl.Color(0xff111111),
                borderRadius: fl.BorderRadius.circular(2),
              ),
              child: widget.child,
            )
          : fl.Tooltip(
              message: widget.message,
              textAlign: fl.TextAlign.center,
              decoration: fl.BoxDecoration(
                color: const fl.Color(0xff111111),
                borderRadius: fl.BorderRadius.circular(2),
              ),
              child: widget.child,
            ),
    );
  }
}
