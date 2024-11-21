import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart' hide State;
import 'package:grpc/grpc.dart' hide ConnectionState;

import '../extensions.dart';
import '../grpc_client.dart';
import '../sidebar.dart';
import 'notifications_list.dart';

void closeNotification(BuildContext context) {
  Actions.maybeInvoke(context, const CloseNotificationIntent());
}

class SimpleNotification extends StatelessWidget {
  final Widget child;
  final Widget icon;
  final Color barColor;
  final bool closeable;
  final double barFullness;

  const SimpleNotification({
    super.key,
    required this.child,
    required this.icon,
    required this.barColor,
    this.closeable = true,
    this.barFullness = 1.0,
  });

  @override
  Widget build(BuildContext context) {
    return Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
      RotatedBox(
        quarterTurns: -1,
        child: LinearProgressIndicator(
          backgroundColor: Colors.transparent,
          color: barColor,
          value: barFullness,
        ),
      ),
      const SizedBox(width: 8),
      Expanded(
        child: Padding(
          padding: const EdgeInsets.symmetric(vertical: 10),
          child: Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
            SizedBox(
              width: 20,
              height: 20,
              child: FittedBox(fit: BoxFit.fill, child: icon),
            ),
            const SizedBox(width: 8),
            Expanded(child: child),
          ]),
        ),
      ),
      if (closeable)
        Material(
          color: Colors.transparent,
          child: IconButton(
            splashRadius: 10,
            iconSize: 20,
            icon: const Icon(Icons.close),
            onPressed: () => closeNotification(context),
          ),
        ),
    ]);
  }
}

class TimeoutNotification extends StatefulWidget {
  final Widget child;
  final Widget icon;
  final Color barColor;
  final Duration duration;

  const TimeoutNotification({
    super.key,
    required this.child,
    required this.icon,
    required this.barColor,
    this.duration = const Duration(seconds: 5),
  });

  @override
  State<TimeoutNotification> createState() => _TimeoutNotificationState();
}

class _TimeoutNotificationState extends State<TimeoutNotification>
    with SingleTickerProviderStateMixin {
  late final AnimationController timeoutController;

  @override
  void initState() {
    super.initState();
    timeoutController = AnimationController(
      vsync: this,
      duration: widget.duration,
    );
    timeoutController.addStatusListener((status) {
      if (status == AnimationStatus.completed) {
        closeNotification(context);
      }
    });
    timeoutController.forward();
  }

  @override
  void dispose() {
    timeoutController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MouseRegion(
      onEnter: (_) => timeoutController.reset(),
      onExit: (_) => timeoutController.forward(),
      child: AnimatedBuilder(
        animation: timeoutController,
        builder: (_, __) => SimpleNotification(
          icon: widget.icon,
          barColor: widget.barColor,
          barFullness: 1.0 - timeoutController.value,
          child: widget.child,
        ),
      ),
    );
  }
}

class ErrorNotification extends SimpleNotification {
  ErrorNotification({
    super.key,
    required String text,
  }) : super(
          child: Text(text),
          barColor: Colors.red,
          icon: const Icon(Icons.cancel_outlined, color: Colors.red),
        );
}

class SuccessNotification extends TimeoutNotification {
  const SuccessNotification({
    super.key,
    required super.child,
  }) : super(
          barColor: Colors.green,
          icon: const Icon(Icons.check_circle_outline, color: Colors.green),
        );
}

class OperationNotification extends StatelessWidget {
  final String text;
  final Future<String> future;

  const OperationNotification({
    super.key,
    required this.future,
    required this.text,
  });

  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
      future: future,
      builder: (_, snapshot) {
        if (snapshot.hasError) {
          return ErrorNotification(text: snapshot.error.toString());
        }

        final data = snapshot.data;
        if (data != null) return SuccessNotification(child: Text(data));

        return SimpleNotification(
          barColor: Colors.blue,
          closeable: false,
          icon: const CircularProgressIndicator(
            color: Colors.blue,
            strokeAlign: -2,
            strokeWidth: 3.5,
          ),
          child: Text(text),
        );
      },
    );
  }
}

class LaunchingNotification extends ConsumerWidget {
  final Stream<Either<LaunchReply, MountReply>?> stream;
  final Completer<void> cancelCompleter;
  final String name;

  const LaunchingNotification({
    super.key,
    required this.stream,
    required this.cancelCompleter,
    required this.name,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return StreamBuilder(
      stream: stream,
      builder: (_, snapshot) {
        final error = snapshot.error;
        if (error != null) {
          final grpcErrorMessage = error is GrpcError ? error.message : null;
          return ErrorNotification(text: grpcErrorMessage ?? error.toString());
        }

        if (snapshot.connectionState == ConnectionState.done) {
          return SuccessNotification(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text.rich([
                  '$name is up and running\n'.span.bold,
                  'You can start using it now'.span,
                ].spans),
                Divider(),
                Row(children: [
                  const Spacer(),
                  TextButton(
                    onPressed: () {
                      ref.read(sidebarKeyProvider.notifier).set('vm-$name');
                      closeNotification(context);
                    },
                    child: Text('Go to instance'),
                  ),
                ]),
              ],
            ),
          );
        }

        // returns the message to be displayed and if the operation is cancelable
        final data = snapshot.data?.match(
          (l) {
            switch (l.whichCreateOneof()) {
              case LaunchReply_CreateOneof.launchProgress:
                final progressType = l.launchProgress.type;
                if (progressType == LaunchProgress_ProgressTypes.VERIFY) {
                  return ('Verifying image', false);
                }

                final downloadPercentage = l.launchProgress.percentComplete;
                return ('Downloading image $downloadPercentage%', true);
              case LaunchReply_CreateOneof.createMessage:
                return (l.createMessage, false);
              default:
                return (l.replyMessage, false);
            }
          },
          (m) => (m.replyMessage, false),
        );

        final (message, cancelable) = data ?? ('', false);

        final notification = SimpleNotification(
          barColor: Colors.blue,
          closeable: false,
          icon: const CircularProgressIndicator(
            color: Colors.blue,
            strokeAlign: -2,
            strokeWidth: 3.5,
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text.rich(['Launching $name\n'.span.bold, message.span].spans),
              if (cancelable) ...[
                const Divider(),
                Row(children: [
                  const Spacer(),
                  OutlinedButton(
                    onPressed: () {
                      closeNotification(context);
                      cancelCompleter.complete();
                    },
                    child: Text('Cancel'),
                  ),
                  const SizedBox(width: 20),
                ])
              ],
            ],
          ),
        );

        return MouseRegion(
          cursor: SystemMouseCursors.click,
          child: GestureDetector(
            onTap: () => ref.read(sidebarKeyProvider.notifier).set('vm-$name'),
            child: notification,
          ),
        );
      },
    );
  }
}
