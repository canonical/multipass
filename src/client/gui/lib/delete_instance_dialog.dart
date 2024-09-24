import 'package:flutter/material.dart';

import 'confirmation_dialog.dart';

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
    return ConfirmationDialog(
      title: 'Delete instance${multiple ? 's' : ''}',
      body: Text(
        "You won't be able to recover ${multiple ? 'these instances' : 'this instance'}.",
      ),
      actionText: 'Delete',
      onAction: () {
        onDelete();
        Navigator.pop(context);
      },
      inactionText: 'Cancel',
      onInaction: () => Navigator.pop(context),
    );
  }
}
