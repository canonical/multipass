import 'package:flutter/material.dart' hide Switch;

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
    return AlertDialog(
      shape: const Border(),
      contentPadding: const EdgeInsets.symmetric(horizontal: 16),
      titlePadding: const EdgeInsets.symmetric(horizontal: 16).copyWith(top: 8),
      buttonPadding: const EdgeInsets.symmetric(horizontal: 16),
      title: Row(children: [
        const Expanded(child: Text('Before quitting')),
        IconButton(
          onPressed: widget.onClose,
          splashRadius: 15,
          icon: const Icon(Icons.close),
        ),
      ]),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Divider(),
          const SizedBox(
            width: 600,
            child: Text(
              'When quitting this application you can leave instances running in the background or choose to stop them completely.',
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 18),
            child: Switch(
              value: remember,
              label: 'Remember this setting',
              onChanged: (value) => setState(() => remember = value),
            ),
          ),
          const Divider(),
        ],
      ),
      actions: [
        OutlinedButton(
          onPressed: () => widget.onKeep(remember),
          child: const Text('Leave instances running'),
        ),
        TextButton(
          onPressed: () => widget.onStop(remember),
          style: TextButton.styleFrom(
            backgroundColor: const Color(0xffC7162B),
          ),
          child: const Text('Stop instances'),
        ),
      ],
    );
  }
}
