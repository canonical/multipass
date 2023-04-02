import 'package:basics/basics.dart';
import 'package:flutter/material.dart';

import 'instance_action.dart';
import 'vms_screen.dart';

void showSnackBarMessage(BuildContext context, String message,
    {bool failure = false}) {
  _showSnackBar(context, _snackBarMessage(message, failure: failure));
}

void showInstanceActionSnackBar(BuildContext context, InstanceAction action) {
  _showSnackBar(context, _instanceActionSnackBarContent(action));
}

void _showSnackBar(
  BuildContext context,
  Widget content,
) {
  vmScreenScaffold.currentState!.clearSnackBars();
  vmScreenScaffold.currentState!.showSnackBar(SnackBar(
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

Widget _snackBarMessage(String text, {bool failure = false}) {
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
        onPressed: () => vmScreenScaffold.currentState!.hideCurrentSnackBar(),
        iconSize: 20,
        padding: EdgeInsets.zero,
        constraints: const BoxConstraints(maxHeight: 20, maxWidth: 20),
        icon: const Icon(Icons.close),
        color: Colors.white,
      ),
    ],
  );
}

Widget _instanceActionSnackBarContent(InstanceAction action) {
  final instances = action.instances.joinWithAnd();
  return FutureBuilder(
    future: action.function(action.instances),
    builder: (_, snapshot) {
      if (snapshot.hasError) {
        return _snackBarMessage(
          'Failed to ${action.name.toLowerCase()} $instances: ${snapshot.error}.',
          failure: true,
        );
      }

      if (snapshot.connectionState == ConnectionState.done) {
        return _snackBarMessage(
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

extension IterableExtension on Iterable<String> {
  String joinWithAnd() {
    final n = length;
    if (n == 0) {
      return '';
    }

    if (n == 1) {
      return first;
    }

    return '${take(n - 1).join(', ')} and $last';
  }
}
