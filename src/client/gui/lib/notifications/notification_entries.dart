import 'package:flutter/material.dart';

import 'notifications_list.dart';

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
            onPressed: () {
              Actions.maybeInvoke(context, const CloseNotificationIntent());
            },
          ),
        ),
    ]);
  }
}

class TimeoutNotification extends StatefulWidget {
  final String text;
  final Widget icon;
  final Color barColor;
  final Duration duration;

  const TimeoutNotification({
    super.key,
    required this.text,
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
        close();
      }
    });
    timeoutController.forward();
  }

  @override
  void dispose() {
    timeoutController.dispose();
    super.dispose();
  }

  void close() => Actions.invoke(context, const CloseNotificationIntent());

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
          child: Text(widget.text),
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
    required super.text,
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

        if (snapshot.hasData) {
          return SuccessNotification(text: snapshot.data!);
        }

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
