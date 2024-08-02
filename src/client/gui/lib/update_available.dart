import 'package:flutter/material.dart';
import 'package:flutter_svg/svg.dart';
import 'package:url_launcher/url_launcher.dart';

import 'grpc_client.dart';
import 'notifications/notification_entries.dart';
import 'notifications/notifications_list.dart';

class UpdateAvailable extends StatelessWidget {
  final UpdateInfo updateInfo;

  const UpdateAvailable(this.updateInfo, {super.key});

  static final installUrl = Uri.parse('https://multipass.run/install');
  static const color = Color(0xffE95420);

  @override
  Widget build(BuildContext context) {
    return SimpleNotification(
      barColor: color,
      icon: SvgPicture.asset(
        'assets/multipass.svg',
        width: 30,
        colorFilter: const ColorFilter.mode(color, BlendMode.srcIn),
      ),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(
          'Multipass ${updateInfo.version} is available',
          style: const TextStyle(fontSize: 16),
        ),
        const SizedBox(height: 12),
        TextButton(
          onPressed: () async {
            await launchUrl(installUrl);
            if (!context.mounted) return;
            Actions.maybeInvoke(context, const CloseNotificationIntent());
          },
          child: const Text('Upgrade now', style: TextStyle(fontSize: 14)),
        ),
      ]),
    );
  }
}
