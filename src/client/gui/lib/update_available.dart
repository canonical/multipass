import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/svg.dart';
import 'package:url_launcher/url_launcher.dart';

import 'notifications/notification_entries.dart';
import 'notifications/notifications_list.dart';
import 'notifications/notifications_provider.dart';
import 'providers.dart';

final updateProvider = FutureProvider.autoDispose((ref) {
  return ref.watch(grpcClientProvider).updateInfo();
});

const _color = Color(0xffE95420);
final installUrl = Uri.parse('https://multipass.run/install');

Future<void> launchInstallUrl() => launchUrl(installUrl);

class UpdateAvailable extends StatelessWidget {
  final UpdateInfo updateInfo;

  const UpdateAvailable(this.updateInfo, {super.key});

  @override
  Widget build(BuildContext context) {
    final icon = Container(
      alignment: Alignment.center,
      color: _color,
      height: 40,
      width: 40,
      child: SvgPicture.asset('assets/multipass.svg', width: 25),
    );

    final text = Text(
      'Multipass ${updateInfo.version} is available',
      style: const TextStyle(fontSize: 16),
    );

    const button = TextButton(
      onPressed: launchInstallUrl,
      child: Text('Upgrade now'),
    );

    return Container(
      color: const Color(0xffF7F7F7),
      padding: const EdgeInsets.all(12),
      child: Row(children: [
        icon,
        const SizedBox(width: 12),
        text,
        const Spacer(),
        button,
      ]),
    );
  }
}

class UpdateAvailableNotification extends StatelessWidget {
  final UpdateInfo updateInfo;

  const UpdateAvailableNotification(this.updateInfo, {super.key});

  @override
  Widget build(BuildContext context) {
    return SimpleNotification(
      barColor: _color,
      icon: SvgPicture.asset(
        'assets/multipass.svg',
        width: 30,
        colorFilter: const ColorFilter.mode(_color, BlendMode.srcIn),
      ),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(
          'Multipass ${updateInfo.version} is available',
          style: const TextStyle(fontSize: 16),
        ),
        const SizedBox(height: 12),
        TextButton(
          onPressed: () async {
            await launchInstallUrl();
            if (!context.mounted) return;
            Actions.maybeInvoke(context, const CloseNotificationIntent());
          },
          child: const Text('Upgrade now', style: TextStyle(fontSize: 14)),
        ),
      ]),
    );
  }
}

extension ShowUpdateExtension on WidgetRef {
  void showUpdateNotification(UpdateInfo updateInfo) {
    if (updateInfo.version.isNotBlank) {
      read(notificationsProvider.notifier)
          .add(UpdateAvailableNotification(updateInfo));
    }
  }
}
