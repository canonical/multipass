import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import 'no_vms.dart';
import 'vms.dart';

class VmTableScreen extends ConsumerWidget {
  static const sidebarKey = 'vms';

  const VmTableScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final hasVms = ref.watch(vmInfosProvider.select((vms) => vms.isNotEmpty));

    return Scaffold(body: hasVms ? const Vms() : const NoVms());
  }
}
