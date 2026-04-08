import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';
import 'l10n/app_localizations.dart';

class CloseTerminalDialog extends StatefulWidget {
  final Function() onYes;
  final Function() onNo;
  final Function(bool doNotAsk) onDoNotAsk;

  const CloseTerminalDialog({
    super.key,
    required this.onYes,
    required this.onNo,
    required this.onDoNotAsk,
  });

  @override
  State<CloseTerminalDialog> createState() => _CloseTerminalDialogState();
}

class _CloseTerminalDialogState extends State<CloseTerminalDialog> {
  var doNotAsk = false;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    return ConfirmationDialog(
      title: l10n.closeTerminalTitle,
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 8),
            child: Text(l10n.closeTerminalBody),
          ),
          const SizedBox(height: 24),
          Row(
            children: [
              Checkbox(
                value: doNotAsk,
                onChanged: (value) => setState(() => doNotAsk = value!),
              ),
              const SizedBox(width: 8),
              Text(l10n.dialogDoNotAskAgain),
            ],
          ),
        ],
      ),
      actionText: l10n.closeTerminalConfirm,
      onAction: () {
        widget.onDoNotAsk(
          doNotAsk,
        ); // Apply "do not ask" setting only when closing
        widget.onYes();
      },
      inactionText: l10n.dialogCancel,
      onInaction: () {
        widget.onNo(); // Don't apply "do not ask" setting when canceling
      },
    );
  }
}
