import 'package:basics/basics.dart';
import 'package:flutter/material.dart';

import 'grpc_client.dart';
import 'l10n/app_localizations.dart';

enum VmAction {
  start,
  stop,
  suspend,
  restart,
  delete,
  recover,
  purge,
  edit;

  String label(AppLocalizations l10n) => switch (this) {
        start => l10n.vmActionStartLabel,
        stop => l10n.vmActionStopLabel,
        suspend => l10n.vmActionSuspendLabel,
        restart => l10n.vmActionRestartLabel,
        delete => l10n.vmActionDeleteLabel,
        recover => l10n.vmActionRecoverLabel,
        purge => l10n.vmActionPurgeLabel,
        edit => l10n.vmActionEditLabel,
      };

  String pastTense(AppLocalizations l10n) => switch (this) {
        start => l10n.vmActionStartPastTense,
        stop => l10n.vmActionStopPastTense,
        suspend => l10n.vmActionSuspendPastTense,
        restart => l10n.vmActionRestartPastTense,
        delete => l10n.vmActionDeletePastTense,
        recover => l10n.vmActionRecoverPastTense,
        purge => l10n.vmActionPurgePastTense,
        edit => l10n.vmActionEditPastTense,
      };

  String continuousTense(AppLocalizations l10n) => switch (this) {
        start => l10n.vmActionStartContinuousTense,
        stop => l10n.vmActionStopContinuousTense,
        suspend => l10n.vmActionSuspendContinuousTense,
        restart => l10n.vmActionRestartContinuousTense,
        delete => l10n.vmActionDeleteContinuousTense,
        recover => l10n.vmActionRecoverContinuousTense,
        purge => l10n.vmActionPurgeContinuousTense,
        edit => l10n.vmActionEditContinuousTense,
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
    final l10n = AppLocalizations.of(context)!;
    final enabled = action.allowedStatuses.containsAny(currentStatuses);
    final onPressed = enabled ? function : null;
    return _buildButton(onPressed, l10n);
  }

  Widget _buildButton(VoidCallback? onPressed, AppLocalizations l10n) {
    return OutlinedButton(
      onPressed: onPressed,
      style: ButtonStyle(
        side: WidgetStateBorderSide.resolveWith(
          (states) => BorderSide(
            color: const Color(
              0xff333333,
            ).withAlpha(states.contains(WidgetState.disabled) ? 128 : 255),
          ),
        ),
      ),
      child: Text(action.label(l10n)),
    );
  }
}
