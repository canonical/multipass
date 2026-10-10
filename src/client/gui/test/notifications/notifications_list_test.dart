import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/notifications/notifications_list.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';

/// A notifier that starts with a pre-seeded list of notifications.
class _PreseededNotifier extends NotificationsNotifier {
  final BuiltList<Widget> _initial;

  _PreseededNotifier(this._initial);

  @override
  BuiltList<Widget> build() => _initial;
}

void main() {
  group('NotificationTile', () {
    testWidgets('renders child notification widget', (tester) async {
      await tester.pumpWidget(
        ProviderScope(
          child: MaterialApp(
            home: Scaffold(
              body: NotificationTile(const Text('tile content')),
            ),
          ),
        ),
      );

      expect(find.text('tile content'), findsOneWidget);
    });

    testWidgets(
        'CloseNotificationIntent removes the notification from the provider',
        (tester) async {
      final notification = const Text('closeable note');
      final container = ProviderContainer(
        overrides: [
          notificationsProvider.overrideWith(
            () => _PreseededNotifier(BuiltList([notification])),
          ),
        ],
      );
      addTearDown(container.dispose);
      container.listen(notificationsProvider, (_, __) {});

      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: container,
          child: MaterialApp(
            home: Scaffold(body: NotificationTile(notification)),
          ),
        ),
      );

      expect(container.read(notificationsProvider), hasLength(1));

      // Dispatch the intent from within the tile's subtree.
      final ctx = tester.element(find.text('closeable note'));
      Actions.invoke(ctx, const CloseNotificationIntent());
      await tester.pump();

      expect(container.read(notificationsProvider), isEmpty);
    });
  });

  group('NotificationList', () {
    testWidgets('renders pre-seeded notification in the provider',
        (tester) async {
      final notification = const Text('initial notification');

      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            notificationsProvider.overrideWith(
              () => _PreseededNotifier(BuiltList([notification])),
            ),
          ],
          child: const MaterialApp(
            home: Scaffold(body: NotificationList()),
          ),
        ),
      );

      expect(find.text('initial notification'), findsOneWidget);
    });

    testWidgets('shows notifications added after the first build',
        (tester) async {
      final container = await pumpList(tester);

      container.read(notificationsProvider.notifier).add(const Text('new'));
      await tester.pumpAndSettle();

      expect(find.text('new'), findsOneWidget);
    });

    testWidgets('hides a notification once it is removed', (tester) async {
      final container = await pumpList(tester);
      final notifier = container.read(notificationsProvider.notifier);
      const notification = Text('to remove');

      notifier.add(notification);
      await tester.pumpAndSettle();
      notifier.remove(notification);
      await tester.pumpAndSettle();

      expect(find.text('to remove'), findsNothing);
    });

    testWidgets('shows a notification added in the same frame as a removal',
        (tester) async {
      final container = await pumpList(tester);
      final notifier = container.read(notificationsProvider.notifier);
      const old = Text('old');

      notifier.add(old);
      await tester.pumpAndSettle();
      notifier.remove(old);
      notifier.add(const Text('new'));
      await tester.pumpAndSettle();

      expect(find.text('old'), findsNothing);
      expect(find.text('new'), findsOneWidget);
    });
  });
}

Future<ProviderContainer> pumpList(WidgetTester tester) async {
  final container = ProviderContainer();
  addTearDown(container.dispose);
  await tester.pumpWidget(
    UncontrolledProviderScope(
      container: container,
      child: const MaterialApp(home: Scaffold(body: NotificationList())),
    ),
  );
  return container;
}
