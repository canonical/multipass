import 'package:built_collection/built_collection.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/notifications/notification_entries.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';

String? errorText(Widget notification) =>
    ((notification as ErrorNotification).child as Text).data;

void main() {
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

    test('initial state is empty', () {
      expect(state(), isEmpty);
    });

    test('add() appends widgets in order', () {
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

    test('addError() shows the error toString() by default', () {
      notifier().addError(Exception('boom'));
      expect(state(), hasLength(1));
      expect(errorText(state().first), equals('Exception: boom'));
    });

    test('addError() shows only the message of a GrpcError', () {
      notifier().addError(GrpcError.internal('grpc error message'));
      expect(errorText(state().first), equals('grpc error message'));
    });

    test('addError() shows the output of a custom format function', () {
      notifier().addError('raw error', (e) => 'formatted: $e');
      expect(errorText(state().first), equals('formatted: raw error'));
    });

    test('addOperation() adds an OperationNotification with the loading text',
        () {
      notifier().addOperation(
        Future.value('result'),
        loading: 'doing work',
        onSuccess: (r) => r,
        onError: (e) => e.toString(),
      );

      expect(state(), hasLength(1));
      final notification = state().first as OperationNotification;
      expect(notification.text, equals('doing work'));
    });

    test('addOperation() maps a successful result through onSuccess', () async {
      notifier().addOperation(
        Future.value(42),
        loading: 'loading',
        onSuccess: (r) => 'success: $r',
        onError: (e) => 'error: $e',
      );

      final notification = state().first as OperationNotification;
      await expectLater(notification.future, completion('success: 42'));
    });

    test('addOperation() passes only the GrpcError message to onError',
        () async {
      notifier().addOperation<String>(
        Future.error(GrpcError.internal('boom')),
        loading: 'loading',
        onSuccess: (r) => r,
        onError: (e) => 'error: $e',
      );

      final notification = state().first as OperationNotification;
      await expectLater(notification.future, throwsA('error: boom'));
    });
  });

  group('ErrorNotificationWidgetRefExtension.notifyError', () {
    testWidgets('adds an ErrorNotification with the formatted error',
        (tester) async {
      final container = ProviderContainer();
      addTearDown(container.dispose);
      container.listen(notificationsProvider, (_, __) {});
      late WidgetRef ref;

      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: container,
          child: Consumer(
            builder: (_, widgetRef, __) {
              ref = widgetRef;
              return const SizedBox();
            },
          ),
        ),
      );

      final handler = ref.notifyError((e) => 'Formatted: $e');
      handler('something went wrong', StackTrace.empty);

      expect(container.read(notificationsProvider), hasLength(1));
      expect(
        errorText(container.read(notificationsProvider).first),
        equals('Formatted: something went wrong'),
      );
    });
  });
}
