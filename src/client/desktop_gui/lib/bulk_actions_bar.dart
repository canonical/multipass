import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'globals.dart';
import 'grpc_client.dart';
import 'instance_action.dart';
import 'vms_screen.dart';

class BulkAction extends InstanceAction {
  final IconData icon;

  BulkAction({
    required this.icon,
    required super.name,
    required super.instances,
    required super.function,
  });
}

class BulkActionsBar extends ConsumerWidget {
  const BulkActionsBar({super.key});

  List<BulkAction> actions(GrpcClient client, Map<String, Status> vms) => [
        BulkAction(
          name: 'Start',
          icon: Icons.play_arrow,
          instances: vms.keys,
          function: client.start,
        ),
        BulkAction(
          name: 'Stop',
          icon: Icons.stop,
          instances: vms.keys,
          function: client.stop,
        ),
        BulkAction(
          name: 'Suspend',
          icon: Icons.pause,
          instances: vms.keys,
          function: client.suspend,
        ),
        BulkAction(
          name: 'Delete',
          icon: Icons.remove,
          instances: vms.keys,
          function: client.delete,
        ),
      ];

  Color _stateColor(Set<MaterialState> states) =>
      states.contains(MaterialState.disabled) ? Colors.white54 : Colors.white;

  Widget _buildButton(
    BuildContext context,
    BulkAction action,
    Set<Status> statuses,
  ) =>
      Container(
        margin: const EdgeInsets.symmetric(horizontal: 4),
        child: OutlinedButton.icon(
          icon: Icon(action.icon),
          label: Text(action.name),
          onPressed: action.allowedStatuses.intersection(statuses).isNotEmpty
              ? () => instanceActionsSnackBar(context, action)
              : null,
          style: ButtonStyle(
            padding: const MaterialStatePropertyAll(EdgeInsets.all(8)),
            iconColor: MaterialStateColor.resolveWith(_stateColor),
            foregroundColor: MaterialStateColor.resolveWith(_stateColor),
            overlayColor: MaterialStateColor.resolveWith(
                (states) => _stateColor(states).withOpacity(0.2)),
            side: MaterialStateBorderSide.resolveWith(
              (states) => BorderSide(color: _stateColor(states)),
            ),
            shape: const MaterialStatePropertyAll(RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(8)),
            )),
          ),
        ),
      );

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClient);
    final vms = ref.watch(selectedVMsProvider).map(infoToStatusMap);
    final statuses = vms.values.toSet();

    return Container(
      color: const Color(0xff313033),
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 4),
      child: Row(
        children: actions(client, vms)
            .map((action) => _buildButton(context, action, statuses))
            .toList(),
      ),
    );
  }
}
