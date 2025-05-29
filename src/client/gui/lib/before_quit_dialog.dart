import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';

class BeforeQuitDialog extends StatefulWidget {
  final Function(bool remember) onStop;
  final Function(bool remember) onKeep;

  const BeforeQuitDialog({
    super.key,
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
    return ConfirmationDialog(
      title: 'Before quitting',
      body: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Padding(
          padding: const EdgeInsets.only(left: 8),
          child: const Text(
            'When quitting this application you can leave instances running in the background or choose to stop them completely.',
          ),
        ),
        const SizedBox(height: 24),
        Row(
          children: [
            Checkbox(
              value: remember,
              onChanged: (value) => setState(() => remember = value!),
            ),
            const SizedBox(width: 8),
            const Text('Remember this setting'),
          ],
        ),
      ]),
      actionText: 'Stop instances',
      onAction: () => widget.onStop(remember),
      inactionText: 'Leave instances running',
      onInaction: () => widget.onKeep(remember),
    );
  }
}
