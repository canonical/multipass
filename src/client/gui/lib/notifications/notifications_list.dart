import 'package:basics/int_basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'notifications_provider.dart';

class NotificationList extends ConsumerStatefulWidget {
  const NotificationList({super.key});

  @override
  ConsumerState<NotificationList> createState() => _NotificationListState();
}

class _NotificationListState extends ConsumerState<NotificationList> {
  final listKey = GlobalKey<AnimatedListState>();
  final activeNotifications = <Widget>[];

  void updateState(BuiltList<Widget> notifications) {
    for (var i = activeNotifications.length; i < notifications.length; i++) {
      listKey.currentState?.insertItem(i);
      activeNotifications.add(notifications[i]);
    }

    for (var i = activeNotifications.length - 1; i >= 0; i--) {
      if (notifications.contains(activeNotifications[i])) continue;
      final notification = activeNotifications.removeAt(i);
      listKey.currentState?.removeItem(i, (_, animation) {
        return SizeTransition(
          sizeFactor: animation,
          axisAlignment: -1.0,
          child: FadeTransition(
            opacity: animation,
            child: NotificationTile(notification),
          ),
        );
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final notifications = ref.watch(notificationsProvider);
    updateState(notifications);

    return DefaultTextStyle(
      style: const TextStyle(fontSize: 16, color: Colors.black),
      child: AnimatedList(
        key: listKey,
        reverse: true,
        shrinkWrap: true,
        initialItemCount: notifications.length,
        itemBuilder: (_, index, animation) {
          return SlideTransition(
            position: animation.drive(Tween(
              begin: const Offset(1, 0),
              end: Offset.zero,
            )),
            child: NotificationTile(notifications[index]),
          );
        },
      ),
    );
  }
}

class CloseNotificationIntent extends Intent {
  const CloseNotificationIntent();
}

class NotificationTile extends ConsumerWidget {
  final Widget notification;

  NotificationTile(this.notification)
      : super(key: GlobalObjectKey(notification));

  void removeSelf(WidgetRef ref) {
    ref.read(notificationsProvider.notifier).remove(notification);
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Actions(
      actions: {
        CloseNotificationIntent: CallbackAction<CloseNotificationIntent>(
          onInvoke: (_) => removeSelf(ref),
        ),
      },
      child: AnimatedSize(
        alignment: Alignment.topCenter,
        duration: 250.milliseconds,
        child: IntrinsicHeight(
          child: Container(
            margin: const EdgeInsets.all(5),
            constraints: const BoxConstraints(minHeight: 60),
            decoration: const BoxDecoration(
              color: Colors.white,
              boxShadow: [BoxShadow(blurRadius: 5, color: Colors.black38)],
            ),
            child: DefaultTextStyle.merge(
              maxLines: 10,
              overflow: TextOverflow.ellipsis,
              child: notification,
            ),
          ),
        ),
      ),
    );
  }
}
