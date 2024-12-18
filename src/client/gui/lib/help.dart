import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

class HelpScreen extends StatelessWidget {
  static const sidebarKey = 'help';

  static final docsUrl = Uri.parse('https://canonical.com/multipass/docs');

  const HelpScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 140).copyWith(top: 40),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Help', style: TextStyle(fontSize: 37)),
            const SizedBox(height: 32),
            const SizedBox(
              width: 530,
              child: Text(
                'View tutorials, how-to guides, and references in our extensive Multipass Documentation site.',
                style: TextStyle(fontSize: 16),
              ),
            ),
            const SizedBox(height: 32),
            TextButton(
              onPressed: () => launchUrl(docsUrl),
              child: const Text('View documentation'),
            ),
          ],
        ),
      ),
    );
  }
}
