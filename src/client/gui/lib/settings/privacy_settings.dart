import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import '../l10n/app_localizations.dart';
import 'setting_widgets.dart';

final telemetryNoticeUrl = Uri.parse(
  'https://canonical.com/legal/systems-information-notice'
  '#metrics-collected-by-multipass-using-ubuntu-insights',
);

// null means the user has not decided yet; it cannot be set back once chosen.
class TelemetryConsentNotifier extends Notifier<bool?> {
  @override
  bool? build() => null;

  void set(bool value) => state = value;
}

final telemetryConsentProvider =
    NotifierProvider<TelemetryConsentNotifier, bool?>(
  TelemetryConsentNotifier.new,
);

class PrivacySettings extends ConsumerWidget {
  const PrivacySettings({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final consent = ref.watch(telemetryConsentProvider);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SettingsSectionTitle(l10n.privacyTitle),
        const SizedBox(height: 20),
        FixedOptionsSetting<bool>(
          label: l10n.privacyTelemetryLabel,
          options: {
            true: l10n.privacyTelemetryOn,
            false: l10n.privacyTelemetryOff,
          },
          value: consent,
          onChanged: ref.read(telemetryConsentProvider.notifier).set,
          subtitle: MouseRegion(
            cursor: SystemMouseCursors.click,
            child: GestureDetector(
              onTap: () => launchUrl(telemetryNoticeUrl),
              child: Text(
                l10n.privacyTelemetryLearnMore,
                style: const TextStyle(
                  fontSize: 12,
                  color: Colors.blue,
                  decoration: TextDecoration.underline,
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}
