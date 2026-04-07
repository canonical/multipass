import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';
import 'l10n/app_localizations.dart';

class DeleteInstanceDialog extends StatelessWidget {
  final VoidCallback onDelete;
  final bool multiple;

  const DeleteInstanceDialog({
    super.key,
    required this.onDelete,
    required this.multiple,
  });

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final count = multiple ? 2 : 1;
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
