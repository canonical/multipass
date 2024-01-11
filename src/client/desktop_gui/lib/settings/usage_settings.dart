import 'package:flutter/material.dart';

class UsageSettings extends StatelessWidget {
  const UsageSettings({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: List.generate(50, (index) => Text('$index')),
    );
  }
}
