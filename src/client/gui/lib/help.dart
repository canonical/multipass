import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

import 'l10n/app_localizations.dart';

class HelpScreen extends StatelessWidget {
  static const sidebarKey = 'help';

  static final docsUrl = Uri.parse('https://canonical.com/multipass/docs');

  const HelpScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 140).copyWith(top: 40),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(l10n.helpLabel, style: const TextStyle(fontSize: 37)),
            const SizedBox(height: 32),
            SizedBox(
              width: 530,
              child: Text(
                l10n.helpBody,
                style: const TextStyle(fontSize: 16),
              ),
            ),
            const SizedBox(height: 32),
            TextButton(
              onPressed: () => launchUrl(docsUrl),
              child: Text(l10n.helpViewDocs),
            ),
          ],
        ),
      ),
    );
  }
}
