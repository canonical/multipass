import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import 'general_settings.dart';
import 'usage_settings.dart';
import 'virtualization_settings.dart';
import 'about_section.dart';

class SettingsScreen extends StatelessWidget {
  static const sidebarKey = 'settings';

  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    const settings = Padding(
      padding: EdgeInsets.only(right: 15),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          GeneralSettings(),
          Divider(height: 60),
          UsageSettings(),
          Divider(height: 60),
          VirtualizationSettings(),
          Divider(height: 60),
          AboutSection(),
        ],
      ),
    );

    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 40, vertical: 40),
        child: SizedBox(
          width: 800,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(l10n.settingsTitle, style: const TextStyle(fontSize: 37)),
              const SizedBox(height: 32),
              const Expanded(child: SingleChildScrollView(child: settings)),
            ],
          ),
        ),
      ),
    );
  }
}
