import 'package:basics/basics.dart';
import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/svg.dart';
import 'package:local_notifier/local_notifier.dart';
import 'package:url_launcher/url_launcher.dart';

import 'colors.dart';
import 'notifications/notification_entries.dart';
import 'notifications/notifications_provider.dart';
import 'l10n/app_localizations.dart';
import 'platform/platform.dart';
import 'providers.dart';

class UpdateNotifier extends Notifier<UpdateInfo> {
  @override
  UpdateInfo build() => UpdateInfo();

  void set(UpdateInfo updateInfo) {
    if (updateInfo.version.isBlank) return;

    final updateNotificationExists = ref.read(notificationsProvider).any((n) {
      return n is UpdateAvailableNotification && n.updateInfo == updateInfo;
    });
    if (updateNotificationExists) return;

    // In-app notification
    ref
        .read(notificationsProvider.notifier)
        .add(UpdateAvailableNotification(updateInfo));

    // Update the state
    state = updateInfo;
  }

  @override
  bool updateShouldNotify(UpdateInfo previous, UpdateInfo next) {
    return previous != next;
  }
}

final updateProvider = NotifierProvider<UpdateNotifier, UpdateInfo>(
  UpdateNotifier.new,
);

final installUrl = Uri.parse('https://canonical.com/multipass/install');

Future<void> launchInstallUrl() => launchUrl(installUrl);

class UpdateSystemNotificationListener extends ConsumerWidget {
  const UpdateSystemNotificationListener({required this.child, super.key});

  final Widget child;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    ref.listen(updateProvider, (_, updateInfo) {
      if (!mpPlatform.showLocalUpdateNotifications) return;
      final l10n = AppLocalizations.of(context)!;
      final notification = LocalNotification(
        title: l10n.localNotificationUpdateTitle,
        body: l10n.localNotificationUpdateBody(updateInfo.version),
      );
      notification.onClick = () async {
        await launchInstallUrl();
        await notification.close();
      };
      notification.show();
    });
    return child;
  }
}

class UpdateAvailable extends StatelessWidget {
  final UpdateInfo updateInfo;

  const UpdateAvailable(this.updateInfo, {super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final colors = context.colors;
    final icon = Container(
      alignment: Alignment.center,
      color: colors.brand,
      height: 40,
      width: 40,
      child: SvgPicture.asset('assets/multipass.svg', width: 25),
    );

    final text = Text(
      l10n.updateAvailableTitle(updateInfo.version),
      style: const TextStyle(fontSize: 16),
    );

    final button = TextButton(
      onPressed: launchInstallUrl,
      child: Text(l10n.updateAvailableUpgrade),
    );

    return Container(
      color: colors.bannerBackground,
      padding: const EdgeInsets.all(12),
      child: Row(
        children: [
          icon,
          const SizedBox(width: 12),
          text,
          const Spacer(),
          button,
        ],
      ),
    );
  }
}

class UpdateAvailableNotification extends StatelessWidget {
  final UpdateInfo updateInfo;

  const UpdateAvailableNotification(this.updateInfo, {super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final brand = context.colors.brand;
    return SimpleNotification(
      barColor: brand,
      icon: SvgPicture.asset(
        'assets/multipass.svg',
        width: 30,
        colorFilter: ColorFilter.mode(brand, BlendMode.srcIn),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            l10n.updateAvailableTitle(updateInfo.version),
            style: const TextStyle(fontSize: 16),
          ),
          const SizedBox(height: 12),
          TextButton(
            onPressed: () async {
              await launchInstallUrl();
              if (!context.mounted) return;
              closeNotification(context);
            },
            child: Text(
              l10n.updateAvailableUpgrade,
              style: const TextStyle(fontSize: 14),
            ),
          ),
        ],
      ),
    );
  }
}
