import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import 'vms.dart';

class VmTableScreen extends ConsumerWidget {
  static const sidebarKey = 'vms';

  const VmTableScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return const Scaffold(body: Vms());
  }
}
