import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import 'grpc_client.dart';
import 'text_span_ext.dart';

void launchDocs() => launchUrl(Uri.parse('https://multipass.run/docs'));

void launchTutorial() {
  if (Platform.isLinux) {
    launchUrl(Uri.parse('https://multipass.run/docs/linux-tutorial'));
  } else if (Platform.isWindows) {
    launchUrl(Uri.parse('https://multipass.run/docs/windows-tutorial'));
  } else if (Platform.isMacOS) {
    launchUrl(Uri.parse('https://multipass.run/docs/mac-tutorial'));
  } else {
    throw const OSError("Platform not supported");
  }
}

final versionProvider = Provider((ref) {
  ref
      .watch(grpcClientProvider)
      .version()
      .then((version) => ref.state = version);
  return '';
});

class HelpScreen extends ConsumerWidget {
  static const sidebarKey = 'help';

  const HelpScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final version = ref.watch(versionProvider);

    final gettingStarted = RichText(
      text: [
        'Getting started with Multipass.\n\n'.span.size(24),
        'Multipass uses Hyper-V on Windows, QEMU and HyperKit on macOS and LXD'
                'on Linux for minimal overhead and the fastest possible start'
                'time. Find out more about '
            .span,
        'platform-relevant tutorial'.span.link(launchTutorial, ref),
        ' and '.span,
        'Multipass documentation.'.span.link(launchDocs, ref),
      ].spans,
    );

    final launchInstance = RichText(
      text: [
        'Launching an instance.\n\n'.span.size(24),
        'In the instance page, click '.span,
        '"Launch new instance".\n'.span.bold,
        '\t1. The '.span,
        'Launch instance'.span.bold,
        ' form is pre-filled with default details to create an instance.\n'
            .span,
        '\t2. If no '.span,
        'instance name'.span,
        ' was given, Multipass will randomly generate a name for that instance.\n'
            .span,
        '\t3. Instances cannot be renamed once they are launched.\n'.span,
        '\t4. The '.span,
        'Launch instance'.span.bold,
        ' form pre-fills '.span,
        'default values'.span.bold,
        ', make changes to reflect the usage as changes are immutable.\n'.span,
        '\t5. Click '.span,
        'Launch'.span.bold,
        ' to launch the instance.\n'.span,
      ].spans,
    );

    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.all(15),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Help',
              style: TextStyle(fontSize: 20, fontWeight: FontWeight.w700),
            ),
            const Divider(),
            gettingStarted,
            const SizedBox(height: 45),
            launchInstance,
            const Spacer(),
            Align(alignment: Alignment.centerRight, child: Text(version)),
          ],
        ),
      ),
    );
  }
}
