import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../l10n/app_localizations.dart';
import '../providers.dart';
import 'mapping_slider.dart';
import 'memory_slider.dart';

class RamSlider extends ConsumerWidget {
  final int? initialValue;
  final int min;
  final FormFieldSetter<int> onSaved;

  RamSlider({super.key, int? min, this.initialValue, required this.onSaved})
      : min = min ?? 512.mebi;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final daemonInfo = ref.watch(daemonInfoProvider);
    final ram = daemonInfo.when(
      data: (data) => data.memory.toInt(),
      loading: () => min,
      error: (_, __) => min,
    );
    final max = math.max(initialValue ?? min, ram);

    return MemorySlider(
      label: l10n.ramSliderLabel,
      initialValue: initialValue,
      min: min,
      max: max,
      sysMax: ram,
      onSaved: onSaved,
    );
  }
}
