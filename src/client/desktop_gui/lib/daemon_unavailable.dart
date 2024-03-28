import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'providers.dart';

class DaemonUnavailable extends ConsumerWidget {
  const DaemonUnavailable({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final available = ref.watch(daemonAvailableProvider);

    final content = Container(
      padding: const EdgeInsets.all(20),
      decoration: const BoxDecoration(
        color: Colors.white,
        boxShadow: [
          BoxShadow(color: Colors.black54, blurRadius: 10, spreadRadius: 5)
        ],
      ),
      child: const Row(mainAxisSize: MainAxisSize.min, children: [
        CircularProgressIndicator(color: Colors.orange),
        SizedBox(width: 20),
        Text('Waiting for daemon...'),
      ]),
    );

    return IgnorePointer(
      ignoring: available,
      child: AnimatedOpacity(
        opacity: available ? 0 : 1,
        duration: const Duration(milliseconds: 500),
        child: Scaffold(
          backgroundColor: Colors.black54,
          body: BackdropFilter(
            filter: const ColorFilter.mode(Colors.grey, BlendMode.saturation),
            child: Center(child: content),
          ),
        ),
      ),
    );
  }
}
