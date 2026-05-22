import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../dropdown.dart';
import '../l10n/app_localizations.dart';
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
    final l10n = AppLocalizations.of(context)!;
    final update = ref.watch(updateProvider);
    final autostart = ref.watch(autostartProvider).when(
          data: (data) => data,
          loading: () => false,
          error: (_, __) => false,
        );
    final onAppClose = ref.watch(onAppCloseProvider);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          l10n.generalTitle,
          style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 20),
        if (update.version.isNotBlank) ...[
          UpdateAvailable(update),
          const SizedBox(height: 20),
        ],
        Switch(
          label: l10n.generalAutostartLabel,
          value: autostart,
          trailingSwitch: true,
          size: 30,
          onChanged: (value) {
            ref.read(autostartProvider.notifier).set(value).onError(
                ref.notifyError((e) => l10n.generalAutostartError('$e')));
          },
        ),
        const SizedBox(height: 20),
        Dropdown(
          label: l10n.generalOnCloseLabel,
          width: 260,
          value: onAppClose ?? 'ask',
          onChanged: (value) =>
              ref.read(onAppCloseProvider.notifier).set(value!),
          items: {
            'ask': l10n.generalOnCloseAsk,
            'stop': l10n.generalOnCloseStop,
            'nothing': l10n.generalOnCloseNothing,
          },
        ),
      ],
    );
  }
}
