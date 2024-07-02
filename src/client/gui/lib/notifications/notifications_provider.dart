import 'dart:async';

import 'package:built_collection/built_collection.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';

import '../notifications.dart';

String objectToString(Object? object) => object.toString();

class NotificationsNotifier extends AutoDisposeNotifier<BuiltList<Widget>> {
  @override
  BuiltList<Widget> build() => BuiltList();

  void add(Widget notification) {
    state = state.rebuild((builder) => builder.add(notification));
  }

  void remove(Widget notification) {
    state = state.rebuild((builder) => builder.remove(notification));
  }

  void addError(
    Object? error, [
    String Function(Object?) format = objectToString,
  ]) {
    if (error is GrpcError) error = error.message;
    add(ErrorNotification(text: format(error)));
  }

  void addOperation<T>(
    Future<T> op, {
    required String loading,
    required String Function(T) onSuccess,
    required String Function(Object?) onError,
  }) {
    add(OperationNotification(
      text: loading,
      future: op.then(onSuccess).onError((error, _) {
        if (error is GrpcError) error = error.message;
        throw onError(error);
      }),
    ));
  }
}

final notificationsProvider =
    NotifierProvider.autoDispose<NotificationsNotifier, BuiltList<Widget>>(
  NotificationsNotifier.new,
);

extension ErrorNotificationWidgetRefExtension on WidgetRef {
  void Function(Object?, StackTrace) notifyError(
    String Function(Object?) onError,
  ) {
    return (error, _) {
      read(notificationsProvider.notifier).addError(error, onError);
    };
  }
}

extension ErrorNotificationRefExtension on Ref {
  void Function(Object?, StackTrace) notifyError(
    String Function(Object?) onError,
  ) {
    return (error, _) {
      read(notificationsProvider.notifier).addError(error, onError);
    };
  }
}
