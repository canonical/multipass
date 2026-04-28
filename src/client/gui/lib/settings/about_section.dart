import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../display_field.dart';
import '../ffi.dart';
import '../l10n/app_localizations.dart';
import '../providers.dart';

class AboutSection extends ConsumerWidget {
  const AboutSection({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final daemonVersion = ref.watch(daemonVersionProvider);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          l10n.aboutTitle,
          style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 20),
        DisplayField(
          label: l10n.aboutVersionLabel,
          width: 260,
          text: multipassVersion,
          copyable: true,
        ),
        if (multipassVersion != daemonVersion) const SizedBox(height: 20),
        if (multipassVersion != daemonVersion)
          DisplayField(
            label: l10n.aboutDaemonVersionLabel,
            width: 260,
            text: daemonVersion,
            copyable: true,
          ),
        const SizedBox(height: 20),
        const DisplayField(
          label: 'Copyright © Canonical, Ltd.',
          width: 260,
          copyable: false,
        ),
      ],
    );
  }
}
