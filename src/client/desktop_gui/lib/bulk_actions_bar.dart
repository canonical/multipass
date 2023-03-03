import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'generated/multipass.pbgrpc.dart';
import 'globals.dart';
import 'grpc_client.dart';
import 'vms_screen.dart';

class BulkAction {
  final String label;
  final IconData icon;
  final VoidCallback? action;

  BulkAction({required this.label, required this.icon, required this.action});
}

class BulkActionsBar extends ConsumerWidget {
  const BulkActionsBar({super.key});

  Iterable<BulkAction> _actions(
    GrpcClient client,
    Map<String, InfoReply_Info> vms,
  ) {
    return [
      BulkAction(
        label: 'Start',
        icon: Icons.play_arrow,
        action: vms.values.any((e) => [Status.STOPPED, Status.SUSPENDED]
                .contains(e.instanceStatus.status))
            ? () => client.start(vms.keys)
            : null,
      ),
      BulkAction(
        label: 'Stop',
        icon: Icons.stop,
        action: vms.values.any((e) => e.instanceStatus.status == Status.RUNNING)
            ? () => client.stop(vms.keys)
            : null,
      ),
      BulkAction(
        label: 'Suspend',
        icon: Icons.pause,
        action: vms.values.any((e) => e.instanceStatus.status == Status.RUNNING)
            ? () => client.suspend(vms.keys)
            : null,
      ),
      BulkAction(
        label: 'Delete',
        icon: Icons.remove,
        action: vms.isEmpty ? null : () => client.delete(vms.keys),
      ),
    ];
  }

  Color _stateColor(Set<MaterialState> states) =>
      states.contains(MaterialState.disabled) ? Colors.white54 : Colors.white;

  Widget _buildButton(BulkAction action) => Container(
        margin: const EdgeInsets.symmetric(horizontal: 4),
        child: OutlinedButton.icon(
            onPressed: action.action,
            icon: Icon(action.icon),
            label: Text(action.label),
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
            )),
      );

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final client = ref.watch(grpcClient);
    final vms = ref.watch(selectedVMsProvider);

    return Container(
      color: const Color(0xff313033),
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 4),
      child: Row(children: _actions(client, vms).map(_buildButton).toList()),
    );
  }
}
