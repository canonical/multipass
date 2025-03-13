import 'package:flutter/material.dart' hide Switch;

import 'confirmation_dialog.dart';
import 'switch.dart';

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
    return ConfirmationDialog(
      title: 'Are you sure you want to close this terminal?',
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text('Its current state will be lost.'),
          const SizedBox(height: 12),
          Switch(
            value: doNotAsk,
            label: 'Do not ask me again.',
            onChanged: (value) => setState(() => doNotAsk = value),
          ),
        ],
      ),
      actionText: 'Yes',
      onAction: () {
        widget.onYes();
        widget.onDoNotAsk(doNotAsk);
      },
      inactionText: 'No',
      onInaction: () {
        widget.onNo();
        widget.onDoNotAsk(doNotAsk);
      },
    );
  }
}
