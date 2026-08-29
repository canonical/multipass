import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/notifications/notifications_list.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';

/// A notifier that starts with a pre-seeded [NotificationList]
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

      await tester.pumpWidget(
        ProviderScope(
          overrides: [
            notificationsProvider.overrideWith(
              () => _PreseededNotifier(BuiltList([notification])),
            ),
          ],
          child: MaterialApp(
            home: Scaffold(
              body: Consumer(
                builder: (_, ref, __) {
                  ref.watch(notificationsProvider);
                  return NotificationTile(notification);
                },
              ),
            ),
          ),
        ),
      );

      final container = ProviderScope.containerOf(
        tester.element(find.byType(Consumer)),
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

    testWidgets(
        'renders an AnimatedList with zero items when provider is empty',
        (tester) async {
      await tester.pumpWidget(
        const ProviderScope(
          child: MaterialApp(
            home: Scaffold(body: NotificationList()),
          ),
        ),
      );

      expect(find.byType(NotificationList), findsOneWidget);
      expect(find.byType(AnimatedList), findsOneWidget);
    });
  });
}
