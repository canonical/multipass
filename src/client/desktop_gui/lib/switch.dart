import 'package:flutter/cupertino.dart';

class Switch extends StatelessWidget {
  final bool value;
  final ValueChanged<bool>? onChanged;
  final String label;

  const Switch({
    super.key,
    required this.value,
    this.onChanged,
    this.label = '',
  });

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      SizedBox(
        height: 25,
        child: FittedBox(
          child: CupertinoSwitch(
            activeColor: CupertinoColors.activeBlue,
            trackColor: const Color(0xffd9d9d9),
            value: value,
            onChanged: onChanged,
          ),
        ),
      ),
      const SizedBox(width: 8),
      Text(label, style: const TextStyle(fontSize: 16)),
    ]);
  }
}
