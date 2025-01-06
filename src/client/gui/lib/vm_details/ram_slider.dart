import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:system_info2/system_info2.dart';

import 'mapping_slider.dart';
import 'memory_slider.dart';

final ramSizeProvider = FutureProvider((ref) {
  return ref
      .watch(grpcClientProvider)
      .daemonInfo()
      .then((r) => r.memory.toInt());
});

class RamSlider extends StatelessWidget {
  static final ram = ref.watch(ramSizeProvider).valueOrNull ?? 512.mebi;//?? SysInfo.getTotalPhysicalMemory();

  final int? initialValue;
  final FormFieldSetter<int> onSaved;

  const RamSlider({
    super.key,
    this.initialValue,
    required this.onSaved,
  });

  @override
  Widget build(BuildContext context) {
    return MemorySlider(
      label: 'Memory',
      initialValue: initialValue,
      min: 512.mebi,
      max: math.max(initialValue ?? 0, ram),
      sysMax: ram,
      onSaved: onSaved,
    );
  }
}
