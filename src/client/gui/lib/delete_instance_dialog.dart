import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';
import 'l10n/app_localizations.dart';

class DeleteInstanceDialog extends StatelessWidget {
  final VoidCallback onDelete;
  final int count;

  const DeleteInstanceDialog({
    super.key,
    required this.onDelete,
    this.count = 1,
  });

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    return ConfirmationDialog(
      title: l10n.deleteInstanceTitle(count),
      body: Text(l10n.deleteInstanceBody(count)),
      actionText: l10n.deleteInstanceConfirm,
      onAction: () {
        onDelete();
        Navigator.pop(context);
      },
      inactionText: l10n.dialogCancel,
      onInaction: () => Navigator.pop(context),
    );
  }
}
