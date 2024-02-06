import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import '../vm_action.dart';
import 'vms.dart';

extension on Iterable<Widget> {
  Iterable<Widget> gap({double? width, double? height}) sync* {
    final thisIterator = iterator;
    final gapBox = SizedBox(width: width, height: height);
    if (thisIterator.moveNext()) {
      yield thisIterator.current;
      while (thisIterator.moveNext()) {
        yield gapBox;
        yield thisIterator.current;
      }
    }
  }
}

class BulkActionsBar extends ConsumerWidget {
  const BulkActionsBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClientProvider);
    final selectedVms = ref.watch(selectedVmsProvider);
    final statuses = ref
        .watch(vmStatusesProvider)
        .asMap()
        .whereKey(selectedVms.contains)
        .values
        .toSet();

    final actions = [
      (VmAction.start, client.start),
      (VmAction.stop, client.stop),
      (VmAction.suspend, client.suspend),
      (VmAction.delete, client.delete),
    ];

    final actionButtons = [
      for (final (action, function) in actions)
        VmActionButton(
          action: action,
          currentStatuses: statuses,
          function: () => function(selectedVms),
        ),
    ];

    return Row(children: actionButtons.gap(width: 8).toList());
  }
}
