import 'package:flutter/material.dart';

class VmDetailsScreen extends StatelessWidget {
  final String name;

  const VmDetailsScreen({
    super.key,
    required this.name,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: Text(name),
      ),
    );
  }
}
