import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'mapping_slider.dart';
import 'memory_slider.dart';

class RamSlider extends ConsumerWidget {


  final int? initialValue;
  final FormFieldSetter<int> onSaved;

  final int maxRam;

  const RamSlider({
    super.key,
    this.initialValue,
    required this.onSaved,
    this.maxRam=1,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {

    final ram = maxRam ?? 512.mebi;

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
