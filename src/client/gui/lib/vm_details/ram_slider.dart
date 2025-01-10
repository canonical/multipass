import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import 'mapping_slider.dart';
import 'memory_slider.dart';

class RamSlider extends ConsumerWidget {
  final int? initialValue;
  final int min;
  final FormFieldSetter<int> onSaved;

  const RamSlider({
    super.key,
    int? min,
    this.initialValue,
    required this.onSaved,
  }) : min = min ?? 512.mebi;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final daemonInfo = ref.watch(daemonInfoProvider);
    final ram = daemonInfo.valueOrNull?.memory.toInt() ?? min;
    final max = math.max(initialValue ?? min, ram);

    return MemorySlider(
      label: 'Memory',
      initialValue: initialValue,
      min: min,
      max: max,
      sysMax: ram,
      onSaved: onSaved,
    );
  }
}
