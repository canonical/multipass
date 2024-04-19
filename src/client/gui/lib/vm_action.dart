import 'package:basics/basics.dart';
import 'package:flutter/material.dart';

import 'grpc_client.dart';

enum VmAction {
  start,
  stop,
  suspend,
  restart,
  delete,
  recover,
  purge,
  edit,
  ;

  String get name => switch (this) {
        start => 'Start',
        stop => 'Stop',
        suspend => 'Suspend',
        restart => 'Restart',
        delete => 'Delete',
        recover => 'Recover',
        purge => 'Purge',
        edit => 'Edit',
      };

  String get pastTense => switch (this) {
        start => 'Started',
        stop => 'Stopped',
        suspend => 'Suspended',
        restart => 'Restarted',
        delete => 'Deleted',
        recover => 'Recovered',
        purge => 'Purged',
        edit => 'Edited',
      };

  String get continuousTense => switch (this) {
        start => 'Starting',
        stop => 'Stopping',
        suspend => 'Suspending',
        restart => 'Restarting',
        delete => 'Deleting',
        recover => 'Recovering',
        purge => 'Purging',
        edit => 'Editing',
      };

  Set<Status> get allowedStatuses => switch (this) {
        start => const {Status.STOPPED, Status.SUSPENDED},
        stop => const {Status.RUNNING},
        suspend => const {Status.RUNNING},
        restart => const {Status.RUNNING},
        delete => const {Status.STOPPED, Status.SUSPENDED, Status.RUNNING},
        recover => const {Status.DELETED},
        purge => const {Status.DELETED},
        edit => const {Status.STOPPED},
      };
}

class VmActionButton extends StatelessWidget {
  final VmAction action;
  final Iterable<Status> currentStatuses;
  final VoidCallback function;

  const VmActionButton({
    super.key,
    required this.action,
    required this.currentStatuses,
    required this.function,
  });

  @override
  Widget build(BuildContext context) {
    final enabled = action.allowedStatuses.containsAny(currentStatuses);
    final onPressed = enabled ? function : null;
    return action == VmAction.delete
        ? _buildDeleteButton(onPressed)
        : _buildButton(onPressed);
  }

  Widget _buildButton(VoidCallback? onPressed) {
    return OutlinedButton(
      onPressed: onPressed,
      style: ButtonStyle(
        side: MaterialStateBorderSide.resolveWith(
          (states) => BorderSide(
            color: const Color(0xff333333).withOpacity(
              states.contains(MaterialState.disabled) ? 0.5 : 1,
            ),
          ),
        ),
      ),
      child: Text(action.name),
    );
  }

  Widget _buildDeleteButton(VoidCallback? onPressed) {
    return TextButton(
      onPressed: onPressed,
      style: TextButton.styleFrom(
        backgroundColor: const Color(0xffC7162B),
      ),
      child: Text(action.name),
    );
  }
}
