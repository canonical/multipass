import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';

class BeforeQuitDialog extends StatefulWidget {
  final int runningCount;
  final Function(bool remember) onStop;
  final Function(bool remember) onKeep;

  const BeforeQuitDialog({
    super.key,
    required this.runningCount,
    required this.onStop,
    required this.onKeep,
  });

  @override
  State<BeforeQuitDialog> createState() => _BeforeQuitDialogState();
}

class _BeforeQuitDialogState extends State<BeforeQuitDialog> {
  bool remember = false;

  @override
  Widget build(BuildContext context) {
    String getMessage() {
      if (widget.runningCount == 1) {
        return 'There is 1 running instance. Do you want to stop it?';
      } else {
        return 'There are ${widget.runningCount} running instances. Do you want to stop them?';
      }
    }

    return ConfirmationDialog(
      title: 'Stop running instances?',
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 8),
            child: Text(getMessage()),
          ),
          const SizedBox(height: 24),
          Row(
            children: [
               Checkbox(
                   value: remember,
                   onChanged: (value) => setState(() => remember = value!),
                ),
                const SizedBox(width: 8),
                Flexible(
                  child: Text(
                    'Remember my choice and do not ask me again when quitting with running instances.',
                      style: const TextStyle(fontSize: 13),
                      ),
                    ),
                  ],
                ),

        ],
      ),
      actionText: 'Stop instances',
      onAction: () => widget.onStop(remember),
      inactionText: 'Leave instances running',
      onInaction: () => widget.onKeep(remember),
    );
  }
}
