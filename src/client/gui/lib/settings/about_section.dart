import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../display_field.dart';
import '../providers.dart';
import '../ffi.dart';

class AboutSection extends ConsumerWidget {
  const AboutSection({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final daemonVersion = ref.watch(daemonVersionProvider);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          'About',
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 20),
        DisplayField(
          label: 'Multipass version',
          width: 260,
          text: multipassVersion,
          copyable: true,
        ),
        if (multipassVersion != daemonVersion) const SizedBox(height: 20),
        if (multipassVersion != daemonVersion)
          DisplayField(
            label: 'Multipass daemon version',
            width: 260,
            text: daemonVersion,
            copyable: true,
          ),
        const SizedBox(height: 20),
        DisplayField(
          label: 'Copyright © Canonical, Ltd.',
          width: 260,
          copyable: false,
        ),
      ],
    );
  }
}
