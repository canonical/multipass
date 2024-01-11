import 'package:flutter/material.dart';

class VirtualizationSettings extends StatelessWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: List.generate(50, (index) => Text('$index')),
    );
  }
}
