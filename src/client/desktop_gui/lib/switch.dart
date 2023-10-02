import 'package:flutter/cupertino.dart';

class Switch extends StatelessWidget {
  final bool value;
  final void Function(bool)? onChanged;

  const Switch({super.key, required this.value, this.onChanged});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 25,
      child: FittedBox(
        child: CupertinoSwitch(
          activeColor: CupertinoColors.activeBlue,
          trackColor: const Color(0xffd9d9d9),
          value: value,
          onChanged: onChanged,
        ),
      ),
    );
  }
}
