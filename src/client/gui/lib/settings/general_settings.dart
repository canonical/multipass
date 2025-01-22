import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../notifications.dart';
import '../providers.dart';
import '../switch.dart';
import '../update_available.dart';
import 'autostart_notifiers.dart';

final onAppCloseProvider = guiSettingProvider(onAppCloseKey);

class GeneralSettings extends ConsumerWidget {
  const GeneralSettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final update = ref.watch(updateProvider);
    final autostart = ref.watch(autostartProvider).valueOrNull ?? false;
    final onAppClose = ref.watch(onAppCloseProvider);

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const Text(
        'General',
        style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
      ),
      const SizedBox(height: 20),
      if (update.version.isNotBlank) ...[
        UpdateAvailable(update),
        const SizedBox(height: 20),
      ],
      Switch(
        label: 'Open the Multipass GUI on startup',
        value: autostart,
        trailingSwitch: true,
        size: 30,
        onChanged: (value) {
          ref
              .read(autostartProvider.notifier)
              .set(value)
              .onError(ref.notifyError((e) => 'Failed to set autostart: $e'));
        },
      ),
      const SizedBox(height: 20),
      Dropdown(
        label: 'When closing Multipass',
        width: 260,
        value: onAppClose ?? 'ask',
        onChanged: (value) => ref.read(onAppCloseProvider.notifier).set(value!),
        items: const {
          'ask': 'Ask about running instances',
          'stop': 'Stop running instances',
          'nothing': 'Do not stop running instances',
        },
      ),
    ]);
  }
}
