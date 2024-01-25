import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:fpdart/fpdart.dart';
import 'package:url_launcher/url_launcher.dart';

import '../providers.dart';
import '../switch.dart';

final updateProvider = Provider.autoDispose((ref) {
  ref.watch(grpcClientProvider).updateInfo().then((value) => ref.state = value);
  return UpdateInfo();
});

final privilegedMountsProvider = daemonSettingProvider(privilegedMountsKey);

class GeneralSettings extends ConsumerWidget {
  const GeneralSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final update = ref.watch(updateProvider);
    final privilegedMounts = ref.watch(privilegedMountsProvider.select((value) {
      return value?.toBoolOption.toNullable() ?? false;
    }));

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(height: 60),
        if (update.version.isNotBlank) UpdateAvailable(update),
        const Text('Multipass', style: TextStyle(fontSize: 16)),
        const SizedBox(height: 28),
        Switch(
          label: 'Open the Multipass GUI on startup',
          value: false,
          onChanged: (value) {},
        ),
        const SizedBox(height: 28),
        Switch(
          label: 'Allow privileged mounts',
          value: privilegedMounts,
          onChanged: (value) {
            ref.read(privilegedMountsProvider.notifier).set(value.toString());
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
      const SizedBox(height: 28),
    ]);
  }
}
