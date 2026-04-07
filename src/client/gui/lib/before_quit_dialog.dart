import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';
import 'l10n/app_localizations.dart';

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
    final l10n = AppLocalizations.of(context)!;
    return ConfirmationDialog(
      title: l10n.beforeQuitTitle,
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 8),
            child: Text(l10n.beforeQuitMessage(widget.runningCount)),
          ),
          const SizedBox(height: 24),
          Row(
            children: [
              Checkbox(
                value: remember,
                onChanged: (value) => setState(() => remember = value!),
              ),
              const SizedBox(width: 8),
              Text(l10n.beforeQuitDoNotAsk),
            ],
          ),
        ],
      ),
      actionText: l10n.beforeQuitStopAction,
      onAction: () => widget.onStop(remember),
      inactionText: l10n.beforeQuitKeepAction,
      onInaction: () => widget.onKeep(remember),
    );
  }
}
