import 'package:flutter/material.dart';

import 'globals.dart';

const actionPastTenses = {
  'Start': 'Started',
  'Stop': 'Stopped',
  'Suspend': 'Suspended',
  'Restart': 'Restarted',
  'Delete': 'Deleted',
  'Recover': 'Recovered',
  'Purge': 'Purged',
};

const actionContinuousTenses = {
  'Start': 'Starting',
  'Stop': 'Stopping',
  'Suspend': 'Suspending',
  'Restart': 'Restarting',
  'Delete': 'Deleting',
  'Recover': 'Recovering',
  'Purge': 'Purging',
};

const actionAllowedStatuses = {
  'Start': {Status.STOPPED, Status.SUSPENDED},
  'Stop': {Status.RUNNING},
  'Suspend': {Status.RUNNING},
  'Restart': {Status.RUNNING},
  'Delete': {Status.STOPPED, Status.SUSPENDED, Status.RUNNING},
  'Recover': {Status.DELETED},
  'Purge': {Status.DELETED},
};

class InstanceAction {
  final String name;
  final String pastTense;
  final String continuousTense;
  final Iterable<String> instances;
  final Set<Status> allowedStatuses;
  final Future<void> Function(Iterable<String>) function;

  InstanceAction({
    required this.name,
    required this.instances,
    required this.function,
  })  : pastTense = actionPastTenses[name]!,
        continuousTense = actionContinuousTenses[name]!,
        allowedStatuses = actionAllowedStatuses[name]!;
}

void instanceActionsSnackBar(BuildContext context, InstanceAction action) {
  final scaffold = ScaffoldMessenger.of(context);
  final instances = action.instances.joinWithAnd();
  final closeButton = IconButton(
    onPressed: () => scaffold.hideCurrentSnackBar(),
    iconSize: 20,
    padding: EdgeInsets.zero,
    constraints: const BoxConstraints(maxHeight: 20, maxWidth: 20),
    icon: const Icon(Icons.close),
    color: Colors.white,
  );

  scaffold.clearSnackBars();
  scaffold.showSnackBar(SnackBar(
      dismissDirection: DismissDirection.none,
      behavior: SnackBarBehavior.floating,
      duration: const Duration(days: 1),
      margin: const EdgeInsets.all(20)
          .copyWith(bottom: MediaQuery.of(context).size.height - 80),
      content: FutureBuilder(
        future: action.function(action.instances),
        builder: (_, snapshot) {
          if (snapshot.hasError) {
            return Row(
              children: [
                const Icon(Icons.circle, color: Colors.red, size: 10),
                const SizedBox(width: 5),
                Text(
                  'Failed to ${action.name.toLowerCase()} $instances: ${snapshot.error}.',
                ),
                const Spacer(),
                closeButton,
              ],
            );
          }

          if (snapshot.connectionState == ConnectionState.done) {
            return Row(
              children: [
                const Icon(Icons.circle, color: Colors.green, size: 10),
                const SizedBox(width: 5),
                Text(
                  'Successfully ${action.pastTense.toLowerCase()} $instances.',
                ),
                const Spacer(),
                closeButton,
              ],
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
      )));
}
