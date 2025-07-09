import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';

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
      title: 'Close tab?',
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 8),
            child: const Text(
              'Are you sure you want to close this tab? Its current state will be lost.',
            ),
          ),
          const SizedBox(height: 24),
          Row(
            children: [
              Checkbox(
                value: doNotAsk,
                onChanged: (value) => setState(() => doNotAsk = value!),
              ),
              const SizedBox(width: 8),
              const Text('Do not ask me again'),
            ],
          ),
        ],
      ),
      actionText: 'Close tab',
      onAction: () {
        widget.onDoNotAsk(
          doNotAsk,
        ); // Apply "do not ask" setting only when closing
        widget.onYes();
      },
      inactionText: 'Cancel',
      onInaction: () {
        widget.onNo(); // Don't apply "do not ask" setting when canceling
      },
    );
  }
}
