import 'dart:math' as math;

import 'package:flutter/material.dart';

class MappingSlider extends StatelessWidget {
  final int min;
  final int max;
  final int value;
  final int Function(int) mapping;
  final int Function(int) inverseMapping;
  final ValueChanged<int> onChanged;
  final bool enabled;

  const MappingSlider({
    super.key,
    required this.min,
    required this.max,
    required this.value,
    this.mapping = nonLinearMapping,
    this.inverseMapping = nonLinearInverseMapping,
    required this.onChanged,
    this.enabled = true,
  });

  @override
  Widget build(BuildContext context) {
    final scaledMin = mapping(min);
    final scaledMax = mapping(max);
    // in case the max cannot be reached because of the mapping, add extra step
    // this will cause the value to actually fall outside of max, but the caller can clamp it
    final extraMaxStep = inverseMapping(scaledMax) < max ? 1 : 0;

    return Slider(
      min: scaledMin.toDouble(),
      max: (scaledMax + extraMaxStep).toDouble(),
      divisions: enabled ? scaledMax - scaledMin + extraMaxStep : null,
      value: mapping(value).toDouble(),
      onChanged: enabled
          ? (mappedValue) => onChanged(inverseMapping(mappedValue.toInt()))
          : null,
    );
  }
}

extension NumToHumanString on num {
  String toHumanString() {
    if (this is int) return toString();
    final asInt = toInt();
    return asInt == this ? asInt.toString() : toStringAsFixed(2);
  }
}

extension BytesFromUnits on num {
  int get kibi => (this * 1024).truncate();

  int get mebi => (kibi * 1024).truncate();

  int get gibi => (mebi * 1024).truncate();
}

num bytesToBytes(num value) => value;

num bytesToKibi(num value) => value / 1.kibi;

num bytesToMebi(num value) => value / 1.mebi;

num bytesToGibi(num value) => value / 1.gibi;

num kibiToBytes(num value) => value.kibi;

num mebiToBytes(num value) => value.mebi;

num gibiToBytes(num value) => value.gibi;

const _sectorSize = 512;
const _scale = 8;

int log2(final int x) => math.log(x) ~/ math.ln2;

int nonLinearMapping(int value) {
  value ~/= _sectorSize;
  final power = log2(value);
  final tickMb = math.pow(2, power);
  final tickMbNext = tickMb * 2;
  final step = (value - tickMb) * _scale ~/ (tickMbNext - tickMb);
  return power * _scale + step;
}

int nonLinearInverseMapping(final int value) {
  final power = value ~/ _scale;
  final step = value % _scale;
  final tickMb = math.pow(2, power) as int;
  final tickMbNext = tickMb * 2;
  final result = tickMb + (tickMbNext - tickMb) * step ~/ _scale;
  return result * _sectorSize;
}
