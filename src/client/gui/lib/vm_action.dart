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

  String label(AppLocalizations l10n) => l10n.vmActionLabel(name);
  String pastTense(AppLocalizations l10n) => l10n.vmActionPastTense(name);
  String continuousTense(AppLocalizations l10n) =>
      l10n.vmActionContinuousTense(name);

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
