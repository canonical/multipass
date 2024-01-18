import 'package:flutter/material.dart';

class DaemonUnavailable extends StatelessWidget {
  final bool available;

  const DaemonUnavailable(this.available, {super.key});

  @override
  Widget build(BuildContext context) {
    const body = Row(mainAxisSize: MainAxisSize.min, children: [
      CircularProgressIndicator(color: Colors.orange),
      SizedBox(width: 20),
      Text('Waiting for daemon...'),
    ]);

    const decoration = BoxDecoration(
      color: Colors.white,
      boxShadow: [
        BoxShadow(color: Colors.black54, blurRadius: 10, spreadRadius: 5)
      ],
    );

    return IgnorePointer(
      ignoring: available,
      child: AnimatedOpacity(
        opacity: available ? 0 : 1,
        duration: const Duration(milliseconds: 500),
        child: Scaffold(
          backgroundColor: Colors.black54,
          body: Center(
            child: Container(
              padding: const EdgeInsets.all(20),
              decoration: decoration,
              child: body,
            ),
          ),
        ),
      ),
    );
  }
}
