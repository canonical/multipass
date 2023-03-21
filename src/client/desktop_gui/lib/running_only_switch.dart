import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'grpc_client.dart';
import 'vms_screen.dart';

class RunningOnlySwitch extends ConsumerWidget {
  const RunningOnlySwitch({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final runningOnly = ref.watch(runningOnlyProvider);
    final hasInstances = ref.watch(
      vmInfosProvider.select((infos) => infos.isNotEmpty),
    );

    return Row(
      children: [
        SizedBox(
          height: 30,
          child: FittedBox(
            child: Switch(
              value: runningOnly,
              onChanged: hasInstances
                  ? (value) =>
                      ref.read(runningOnlyProvider.notifier).state = value
                  : null,
            ),
          ),
        ),
        const Text('Only show running instances.'),
      ],
    );
  }
}
