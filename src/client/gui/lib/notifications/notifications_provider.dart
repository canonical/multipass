import 'package:built_collection/built_collection.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../notifications.dart';

class NotificationsNotifier extends AutoDisposeNotifier<BuiltList<Widget>> {
  @override
  BuiltList<Widget> build() => BuiltList();

  void add(Widget notification) {
    state = state.rebuild((builder) => builder.add(notification));
  }

  void remove(Widget notification) {
    state = state.rebuild((builder) => builder.remove(notification));
  }

  void addError(String text) => add(ErrorNotification(text: text));
}

final notificationsProvider =
    NotifierProvider.autoDispose<NotificationsNotifier, BuiltList<Widget>>(
  NotificationsNotifier.new,
);
