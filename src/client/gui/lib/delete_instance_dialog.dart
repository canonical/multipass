import 'package:flutter/material.dart' hide Switch;

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
    return AlertDialog(
      shape: const Border(),
      contentPadding: const EdgeInsets.symmetric(horizontal: 16),
      titlePadding: const EdgeInsets.symmetric(horizontal: 16).copyWith(top: 8),
      buttonPadding: const EdgeInsets.symmetric(horizontal: 16),
      title: Row(children: [
        Expanded(child: Text('Delete instance${multiple ? 's' : ''}')),
        IconButton(
          onPressed: () => Navigator.pop(context),
          splashRadius: 15,
          icon: const Icon(Icons.close),
        ),
      ]),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Divider(),
          SizedBox(
            width: 350,
            child: Padding(
              padding: const EdgeInsets.symmetric(vertical: 8),
              child: Text(
                "You won't be able to recover ${multiple ? 'these instances' : 'this instance'}.",
              ),
            ),
          ),
          const Divider(),
        ],
      ),
      actions: [
        TextButton(
          onPressed: () {
            onDelete();
            Navigator.pop(context);
          },
          style: TextButton.styleFrom(
            backgroundColor: const Color(0xffC7162B),
          ),
          child: const Text('Delete'),
        ),
        OutlinedButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
      ],
    );
  }
}
