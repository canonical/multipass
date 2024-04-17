import 'package:built_collection/built_collection.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class NotificationsNotifier extends Notifier<BuiltList<Widget>> {
  @override
  BuiltList<Widget> build() => BuiltList();

  void add(Widget notification) {
    state = state.rebuild((builder) => builder.add(notification));
  }

  void remove(Widget notification) {
    state = state.rebuild((builder) => builder.remove(notification));
  }
}

final notificationsProvider =
    NotifierProvider<NotificationsNotifier, BuiltList<Widget>>(
  NotificationsNotifier.new,
);
