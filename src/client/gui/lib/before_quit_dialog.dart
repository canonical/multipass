import 'package:flutter/material.dart' hide Switch;

import 'confirmation_dialog.dart';
import 'switch.dart';

class BeforeQuitDialog extends StatefulWidget {
  final Function(bool remember) onStop;
  final Function(bool remember) onKeep;
  final Function() onClose;

  const BeforeQuitDialog({
    super.key,
    required this.onStop,
    required this.onKeep,
    required this.onClose,
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
      body: Column(children: [
        const Text(
          'When quitting this application you can leave instances running in the background or choose to stop them completely.',
        ),
        const SizedBox(height: 12),
        Switch(
          value: remember,
          label: 'Remember this setting',
          onChanged: (value) => setState(() => remember = value),
        ),
      ]),
      actionText: 'Stop instances',
      onAction: () => widget.onStop(remember),
      inactionText: 'Leave instances running',
      onInaction: () => widget.onKeep(remember),
      width: 500,
    );
  }
}
