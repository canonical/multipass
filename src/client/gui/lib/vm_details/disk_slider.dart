import 'dart:math' as math;

import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import '../tooltip.dart';
import 'mapping_slider.dart';
import 'memory_slider.dart';

final diskSizeProvider = FutureProvider((ref) {
  return ref
      .watch(grpcClientProvider)
      .daemonInfo()
      .then((r) => r.availableSpace.toInt());
});

class DiskSlider extends ConsumerWidget {
  final int? initialValue;
  final int min;
  final FormFieldSetter<int> onSaved;

  DiskSlider({
    super.key,
    int? min,
    this.initialValue,
    required this.onSaved,
  }) : min = min ?? 1.gibi;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final disk = ref.watch(diskSizeProvider).valueOrNull ?? min;
    final max = math.max(initialValue ?? 0, disk);
    final enabled = min != max;

    return Tooltip(
      message: 'Disk size cannot be decreased',
      visible: !enabled,
      child: MemorySlider(
        label: 'Disk',
        enabled: enabled,
        initialValue: initialValue,
        min: min,
        max: max,
        sysMax: disk,
        onSaved: onSaved,
      ),
    );
  }
}
