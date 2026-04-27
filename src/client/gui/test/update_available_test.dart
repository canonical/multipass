import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/update_available.dart';

import 'helpers.dart';

void main() {
  group('UpdateNotifier', () {
    late ProviderContainer container;

    setUp(() {
      container = ProviderContainer();
      // Keep the autoDispose notificationsProvider alive for the duration of each test.
      container.listen(notificationsProvider, (_, __) {});
    });

    tearDown(() {
      container.dispose();
    });

    UpdateInfo state() => container.read(updateProvider);
    UpdateNotifier notifier() => container.read(updateProvider.notifier);
    BuiltList<Widget> notifications() => container.read(notificationsProvider);

    test('initial state is an empty UpdateInfo', () {
      expect(state(), equals(UpdateInfo()));
    });

    test('set() with default UpdateInfo (empty version) does not update state',
        () {
      notifier().set(UpdateInfo());
      expect(state(), equals(UpdateInfo()));
    });

    test(
        'set() with explicitly blank version does not update state or add notification',
        () {
      notifier().set(UpdateInfo(version: ''));
      expect(state(), equals(UpdateInfo()));
      expect(notifications(), isEmpty);
    });

    test('set() updates state', () {
      final info = UpdateInfo(version: '1.15.0');
      notifier().set(info);
      expect(state(), equals(info));
    });

    test('set() adds an UpdateAvailableNotification', () {
      final info = UpdateInfo(version: '1.15.0');
      notifier().set(info);
      expect(notifications(), hasLength(1));
      expect(notifications().first, isA<UpdateAvailableNotification>());
      final notification = notifications().first as UpdateAvailableNotification;
      expect(notification.updateInfo, equals(info));
    });

    test(
        'set() called twice with the same UpdateInfo only adds one notification',
        () {
      final info = UpdateInfo(version: '1.15.0');
      notifier().set(info);
      notifier().set(info);
      expect(notifications(), hasLength(1));
    });

    test(
        'set() called with two different versions adds a notification for each',
        () {
      notifier().set(UpdateInfo(version: '1.15.0'));
      notifier().set(UpdateInfo(version: '1.16.0'));
      expect(notifications(), hasLength(2));
    });

    test('updateShouldNotify returns true when versions differ', () {
      final previous = UpdateInfo(version: '1.14.0');
      final next = UpdateInfo(version: '1.15.0');
      expect(notifier().updateShouldNotify(previous, next), isTrue);
    });

    test('updateShouldNotify returns false when versions are the same', () {
      final previous = UpdateInfo(version: '1.15.0');
      final next = UpdateInfo(version: '1.15.0');
      expect(notifier().updateShouldNotify(previous, next), isFalse);
    });
  });

  group('UpdateAvailable widget', () {
    Widget buildApp(UpdateInfo info) {
      return withFakeSvgAssetBundle(
        MaterialApp(
          locale: const Locale('en'),
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          supportedLocales: AppLocalizations.supportedLocales,
          home: Scaffold(body: UpdateAvailable(info)),
        ),
      );
    }

    testWidgets('displays the version in the title text', (tester) async {
      await tester.pumpWidget(buildApp(UpdateInfo(version: '1.15.0')));
      await tester.pumpAndSettle();
      expect(
        find.text('Multipass 1.15.0 is available'),
        findsOneWidget,
      );
    });

    testWidgets('displays the upgrade button', (tester) async {
      await tester.pumpWidget(buildApp(UpdateInfo(version: '1.15.0')));
      await tester.pumpAndSettle();
      expect(find.text('Upgrade now'), findsOneWidget);
    });
  });

  group('UpdateAvailableNotification widget', () {
    Widget buildApp(UpdateInfo info) {
      return withFakeSvgAssetBundle(
        ProviderScope(
          child: MaterialApp(
            locale: const Locale('en'),
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(body: UpdateAvailableNotification(info)),
          ),
        ),
      );
    }

    testWidgets('displays the version in the notification text',
        (tester) async {
      await tester.pumpWidget(buildApp(UpdateInfo(version: '2.0.0')));
      await tester.pumpAndSettle();
      expect(
        find.text('Multipass 2.0.0 is available'),
        findsOneWidget,
      );
    });

    testWidgets('displays the upgrade button', (tester) async {
      await tester.pumpWidget(buildApp(UpdateInfo(version: '2.0.0')));
      await tester.pumpAndSettle();
      expect(find.text('Upgrade now'), findsOneWidget);
    });
  });
}
