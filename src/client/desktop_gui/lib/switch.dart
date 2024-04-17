import 'package:flutter/cupertino.dart';

class Switch extends StatelessWidget {
  final bool value;
  final ValueChanged<bool>? onChanged;
  final String label;
  final bool trailingSwitch;
  final double size;

  const Switch({
    super.key,
    required this.value,
    this.onChanged,
    this.label = '',
    this.trailingSwitch = false,
    this.size = 25,
  });

  @override
  Widget build(BuildContext context) {
    final switchBox = SizedBox(
      height: size,
      child: FittedBox(
        child: CupertinoSwitch(
          activeColor: CupertinoColors.activeBlue,
          trackColor: const Color(0xffd9d9d9),
          value: value,
          onChanged: onChanged,
        ),
      ),
    );

    final labelText = Text(label, style: const TextStyle(fontSize: 16));

    return trailingSwitch
        ? Row(children: [
            Expanded(child: labelText),
            const SizedBox(width: 8),
            switchBox,
          ])
        : Row(children: [
            switchBox,
            const SizedBox(width: 8),
            labelText,
          ]);
  }
}
