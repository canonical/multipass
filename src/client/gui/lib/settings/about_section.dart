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
          label: 'multipass version:',
          labelStyle: TextStyle(
            fontFamily: 'monospace',
            fontSize: 16,
            color: Colors.black87,
          ),
          width: 260,
          text: multipassVersion,
          copyable: true,
        ),
        const SizedBox(height: 20),
        DisplayField(
          label: 'multipassd version:',
          labelStyle: TextStyle(
            fontFamily: 'monospace',
            fontSize: 16,
            color: Colors.black87,
          ),
          width: 260,
          text: daemonVersion,
          copyable: true,
        ),
        const SizedBox(height: 20),
        DisplayField(
          label: 'Copyright (C) Canonical, Ltd.',
          labelStyle: TextStyle(
            fontSize: 16,
            color: Colors.grey,
          ),
          width: 260,
          copyable: false,
        ),
      ],
    );
  }
}
