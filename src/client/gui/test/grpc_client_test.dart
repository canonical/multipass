import 'package:built_collection/built_collection.dart';
import 'package:protobuf/protobuf.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/update_available.dart';

void main() {
  late ProviderSubscription<BuiltList<Widget>> notificationsSubscription;

  setUpAll(() {
    providerContainer = ProviderContainer();
  });

  setUp(() {
    notificationsSubscription =
        providerContainer.listen(notificationsProvider, (_, __) {});
  });

  tearDown(() {
    notificationsSubscription.close();
    providerContainer.invalidate(updateProvider);
  });

  tearDownAll(() {
    providerContainer.dispose();
  });

  group('checkForUpdate', () {
    void runReplyTests(List<(String, GeneratedMessage)> cases) {
      for (final (name, reply) in cases) {
        test('sets updateProvider from a $name', () {
          checkForUpdate(reply);
          expect(
            providerContainer.read(updateProvider).version,
            equals('1.15.0'),
          );
        });
      }
    }

    final info = UpdateInfo()..version = '1.15.0';
    runReplyTests([
      ('LaunchReply', LaunchReply()..updateInfo = info),
      ('InfoReply', InfoReply()..updateInfo = info),
      ('ListReply', ListReply()..updateInfo = info),
      ('NetworksReply', NetworksReply()..updateInfo = info),
      ('StartReply', StartReply()..updateInfo = info),
      ('RestartReply', RestartReply()..updateInfo = info),
      ('VersionReply', VersionReply()..updateInfo = info),
    ]);

    test('does not update the provider for an unrecognised message type', () {
      checkForUpdate(FindReply());
      expect(providerContainer.read(updateProvider), equals(UpdateInfo()));
    });

    test('ignores a message whose version is blank', () {
      checkForUpdate(LaunchReply()..updateInfo = (UpdateInfo()..version = ''));
      expect(providerContainer.read(updateProvider), equals(UpdateInfo()));
    });

    test('adds an UpdateAvailableNotification for a valid update', () {
      checkForUpdate(
          LaunchReply()..updateInfo = (UpdateInfo()..version = '1.22.0'));
      final notifications = providerContainer.read(notificationsProvider);
      expect(notifications, hasLength(1));
      expect(notifications.first, isA<UpdateAvailableNotification>());
    });

    test('does not add a duplicate notification for the same version', () {
      final info = UpdateInfo()..version = '1.23.0';
      checkForUpdate(LaunchReply()..updateInfo = info);
      checkForUpdate(InfoReply()..updateInfo = info);
      expect(providerContainer.read(notificationsProvider), hasLength(1));
    });
  });
}
