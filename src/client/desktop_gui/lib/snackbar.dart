import 'package:basics/basics.dart';
import 'package:flutter/material.dart';

import 'globals.dart';
import 'instance_action.dart';

void showSnackBarMessage(BuildContext context, String message,
    {bool failure = false}) {
  final scaffold = ScaffoldMessenger.of(context);
  final content = _snackBarMessage(scaffold, message, failure: failure);
  _showSnackBar(context, scaffold, content);
}

void showInstanceActionSnackBar(BuildContext context, InstanceAction action) {
  final scaffold = ScaffoldMessenger.of(context);
  final content = _instanceActionSnackBarContent(context, action);
  _showSnackBar(context, scaffold, content);
}

void _showSnackBar(
  BuildContext context,
  ScaffoldMessengerState scaffold,
  Widget content,
) {
  scaffold.clearSnackBars();
  scaffold.showSnackBar(SnackBar(
    elevation: 0,
    backgroundColor: Colors.transparent,
    dismissDirection: DismissDirection.none,
    behavior: SnackBarBehavior.floating,
    duration: 1.days,
    margin: const EdgeInsets.all(20)
        .copyWith(bottom: MediaQuery.of(context).size.height - 80),
    content: Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xff313033),
        borderRadius: BorderRadius.circular(8),
        boxShadow: const [BoxShadow(blurRadius: 2)],
      ),
      child: content,
    ),
  ));
}

Widget _snackBarMessage(ScaffoldMessengerState scaffold, String text,
    {bool failure = false}) {
  return Row(
    crossAxisAlignment: CrossAxisAlignment.center,
    children: [
      Icon(Icons.circle, color: failure ? Colors.red : Colors.green, size: 10),
      Expanded(
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8),
          child: Text(text),
        ),
      ),
      IconButton(
        onPressed: () => scaffold.hideCurrentSnackBar(),
        iconSize: 20,
        padding: EdgeInsets.zero,
        constraints: const BoxConstraints(maxHeight: 20, maxWidth: 20),
        icon: const Icon(Icons.close),
        color: Colors.white,
      ),
    ],
  );
}

Widget _instanceActionSnackBarContent(
  BuildContext context,
  InstanceAction action,
) {
  final scaffold = ScaffoldMessenger.of(context);
  final instances = action.instances.joinWithAnd();
  return FutureBuilder(
    future: action.function(action.instances),
    builder: (_, snapshot) {
      if (snapshot.hasError) {
        return _snackBarMessage(
          scaffold,
          'Failed to ${action.name.toLowerCase()} $instances: ${snapshot.error}.',
          failure: true,
        );
      }

      if (snapshot.connectionState == ConnectionState.done) {
        return _snackBarMessage(
          scaffold,
          'Successfully ${action.pastTense.toLowerCase()} $instances.',
        );
      }

      return Row(
        children: [
          const SizedBox(
            width: 20,
            height: 20,
            child: CircularProgressIndicator(strokeWidth: 3),
          ),
          const SizedBox(width: 10),
          Text('${action.continuousTense} $instances...'),
        ],
      );
    },
  );
}
