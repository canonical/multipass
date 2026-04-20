import 'package:built_collection/built_collection.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/notifications/notification_entries.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';

void main() {
  group('objectToString', () {
    test('returns the toString() of the argument', () {
      expect(objectToString(42), equals('42'));
      expect(objectToString('hello'), equals('hello'));
      expect(objectToString(null), equals('null'));
      expect(objectToString([1, 2, 3]), equals('[1, 2, 3]'));
    });
  });

  group('NotificationsNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
      // Keep autoDispose provider alive for the duration of each test.
      container.listen(notificationsProvider, (_, __) {});
    });

    tearDown(() {
      container.dispose();
    });

    BuiltList<Widget> state() => container.read(notificationsProvider);
    NotificationsNotifier notifier() =>
        container.read(notificationsProvider.notifier);

    test('initial state is an empty BuiltList', () {
      expect(state(), isEmpty);
      expect(state(), isA<BuiltList<Widget>>());
    });

    test('add() appends a widget to the list', () {
      final widget = const SizedBox();
      notifier().add(widget);
      expect(state(), hasLength(1));
      expect(state().first, same(widget));
    });

    test('add() multiple times grows the list in order', () {
      final first = const SizedBox(key: ValueKey('first'));
      final second = const SizedBox(key: ValueKey('second'));
      final third = const SizedBox(key: ValueKey('third'));
      notifier().add(first);
      notifier().add(second);
      notifier().add(third);
      expect(state(), hasLength(3));
      expect(state()[0], same(first));
      expect(state()[1], same(second));
      expect(state()[2], same(third));
    });

    test('remove() removes the exact widget object from the list', () {
      final widget = const SizedBox();
      notifier().add(widget);
      expect(state(), hasLength(1));
      notifier().remove(widget);
      expect(state(), isEmpty);
    });

    test('remove() of a non-existent widget leaves the list unchanged', () {
      final kept = const SizedBox(key: ValueKey('kept'));
      final other = const SizedBox(key: ValueKey('other'));
      notifier().add(kept);
      notifier().remove(other);
      expect(state(), hasLength(1));
      expect(state().first, same(kept));
    });

    test('addError() with a plain object adds an ErrorNotification', () {
      notifier().addError('something went wrong');
      expect(state(), hasLength(1));
      expect(state().first, isA<ErrorNotification>());
    });

    test('addError() with a plain object uses objectToString by default', () {
      const error = 'plain error';
      notifier().addError(error);
      // The default format is objectToString which calls .toString().
      expect(state(), hasLength(1));
      expect(state().first, isA<ErrorNotification>());
    });

    test(
        'addError() with a GrpcError extracts the message instead of using the '
        'full error string', () {
      const message = 'grpc error message';
      final grpcError = GrpcError.internal(message);

      var capturedArg = Object();
      notifier().addError(grpcError, (e) {
        capturedArg = e!;
        return e.toString();
      });

      // The notifier must have unwrapped the GrpcError to its message before
      // calling the format function.
      expect(capturedArg, equals(message));
      expect(state(), hasLength(1));
      expect(state().first, isA<ErrorNotification>());
    });

    test('addError() with a custom format function calls it with the error',
        () {
      const error = 'raw error';
      var called = false;
      String? captured;

      notifier().addError(error, (e) {
        called = true;
        captured = e as String?;
        return 'formatted: $e';
      });

      expect(called, isTrue);
      expect(captured, equals(error));
      expect(state(), hasLength(1));
      expect(state().first, isA<ErrorNotification>());
    });

    test('addOperation() adds an OperationNotification to the list', () {
      final future = Future.value('done');
      notifier().addOperation(
        future,
        loading: 'loading...',
        onSuccess: (result) => 'success: $result',
        onError: (e) => 'error: $e',
      );
      expect(state(), hasLength(1));
      expect(state().first, isA<OperationNotification>());
    });

    test('addOperation() OperationNotification carries the loading text', () {
      const loadingText = 'doing work';
      final future = Future.value('result');
      notifier().addOperation(
        future,
        loading: loadingText,
        onSuccess: (r) => r,
        onError: (e) => e.toString(),
      );

      final notification = state().first as OperationNotification;
      expect(notification.text, equals(loadingText));
    });
  });
}
