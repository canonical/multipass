import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:url_launcher/url_launcher.dart';

import '../dropdown.dart';
import '../notifications/notifications_provider.dart';
import '../providers.dart';
import '../switch.dart';
import 'autostart_notifiers.dart';

final updateProvider = Provider.autoDispose((ref) {
  ref
      .watch(grpcClientProvider)
      .updateInfo()
      .then((value) => ref.state = value)
      .ignore();
  return UpdateInfo();
});

final onAppCloseProvider = guiSettingProvider(onAppCloseKey);

class GeneralSettings extends ConsumerWidget {
  const GeneralSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final update = ref.watch(updateProvider);
    final autostart = ref.watch(autostartProvider).valueOrNull ?? false;
    final onAppClose = ref.watch(onAppCloseProvider);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          'General',
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 20),
        if (update.version.isNotBlank) UpdateAvailable(update),
        Switch(
          label: 'Open the Multipass GUI on startup',
          value: autostart,
          trailingSwitch: true,
          size: 30,
          onChanged: (value) {
            void onError(Object error, _) => ref
                .read(notificationsProvider.notifier)
                .addError('Failed to set autostart: $error');
            ref.read(autostartProvider.notifier).set(value).onError(onError);
          },
        ),
        const SizedBox(height: 20),
        Dropdown(
          label: 'On close of application',
          width: 260,
          value: onAppClose ?? 'ask',
          onChanged: (value) =>
              ref.read(onAppCloseProvider.notifier).set(value!),
          items: const {
            'ask': 'Ask about running instances',
            'stop': 'Stop running instances',
            'nothing': 'Do not stop running instances',
          },
        ),
      ],
    );
  }
}

class UpdateAvailable extends StatelessWidget {
  final UpdateInfo updateInfo;

  const UpdateAvailable(this.updateInfo, {super.key});

  static final installUrl = Uri.parse('https://multipass.run/install');

  static void launchInstallUrl() => launchUrl(installUrl);

  @override
  Widget build(BuildContext context) {
    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const Text('Update available', style: TextStyle(fontSize: 16)),
      const SizedBox(height: 8),
      Container(
        color: const Color(0xffF7F7F7),
        padding: const EdgeInsets.all(12),
        width: 480,
        child: Row(children: [
          Container(
            alignment: Alignment.center,
            color: const Color(0xffE95420),
            height: 48,
            width: 48,
            child: SvgPicture.asset('assets/multipass.svg', width: 30),
          ),
          const SizedBox(width: 12),
          Text(
            'Multipass ${updateInfo.version}\nis available',
            style: const TextStyle(fontSize: 16),
          ),
          const SizedBox(width: 12),
          const Spacer(),
          const TextButton(
            onPressed: launchInstallUrl,
            child: Text('Upgrade now'),
          ),
        ]),
      ),
      const SizedBox(height: 20),
    ]);
  }
}
