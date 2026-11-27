import 'package:flutter/cupertino.dart';

import 'colors.dart';

class Switch extends StatelessWidget {
  final bool value;
  final ValueChanged<bool>? onChanged;
  final String label;
  final bool trailingSwitch;
  final double size;
  final bool enabled;

  const Switch({
    super.key,
    required this.value,
    this.onChanged,
    this.label = '',
    this.trailingSwitch = false,
    this.size = 25,
    this.enabled = true,
  });

  @override
  Widget build(BuildContext context) {
    final colors = context.colors;
    final switchBox = SizedBox(
      height: size,
      child: FittedBox(
        child: CupertinoSwitch(
          activeTrackColor: colors.switchActiveTrack,
          inactiveTrackColor: colors.switchInactiveTrack,
          value: value,
          onChanged: enabled ? onChanged : null,
        ),
      ),
    );

    final labelText = Text(label, style: const TextStyle(fontSize: 16));

    return trailingSwitch
        ? Row(
            children: [
              Expanded(child: labelText),
              const SizedBox(width: 8),
              switchBox,
            ],
          )
        : Row(children: [switchBox, const SizedBox(width: 8), labelText]);
  }
}
