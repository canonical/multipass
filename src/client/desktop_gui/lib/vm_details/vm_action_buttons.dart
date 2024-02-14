import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../extensions.dart';
import '../providers.dart';
import '../vm_action.dart';

class VmActionButtons extends ConsumerWidget {
  final String name;
  final Status status;

  const VmActionButtons({
    super.key,
    required this.name,
    required this.status,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);

    final actions = [
      (VmAction.edit, () => Scaffold.of(context).openEndDrawer()),
      (VmAction.start, () => client.start([name])),
      (VmAction.stop, () => client.stop([name])),
      (VmAction.suspend, () => client.suspend([name])),
      (VmAction.delete, () => client.delete([name])),
    ];

    final actionButtons = [
      for (final (action, function) in actions)
        VmActionButton(
          action: action,
          currentStatuses: {status},
          function: function,
        ),
    ];

    return Row(children: actionButtons.gap(width: 8).toList());
  }
}
