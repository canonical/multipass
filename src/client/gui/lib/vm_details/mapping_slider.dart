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
  // prints decimals only if they exist, otherwise print as int
  String toNiceString() {
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

class CustomTrackShape extends RectangularSliderTrackShape {
  @override
  Rect getPreferredRect({
    required RenderBox parentBox,
    Offset offset = Offset.zero,
    required SliderThemeData sliderTheme,
    bool isEnabled = false,
    bool isDiscrete = false,
  }) {
    const horizontalPadding = 3;
    final trackHeight = sliderTheme.trackHeight!;
    return Rect.fromLTWH(
      offset.dx + horizontalPadding,
      offset.dy + (parentBox.size.height - trackHeight) / 2,
      parentBox.size.width - 2 * horizontalPadding,
      trackHeight,
    );
  }
}

class CustomThumbShape extends RoundSliderThumbShape {
  CustomThumbShape() : super(enabledThumbRadius: 8, pressedElevation: 4);

  @override
  void paint(
    PaintingContext context,
    Offset center, {
    required Animation<double> activationAnimation,
    required Animation<double> enableAnimation,
    required bool isDiscrete,
    required TextPainter labelPainter,
    required RenderBox parentBox,
    required SliderThemeData sliderTheme,
    required TextDirection textDirection,
    required double value,
    required double textScaleFactor,
    required Size sizeWithOverflow,
  }) {
    super.paint(
      context,
      center,
      activationAnimation: activationAnimation,
      enableAnimation: enableAnimation,
      isDiscrete: isDiscrete,
      labelPainter: labelPainter,
      parentBox: parentBox,
      sliderTheme: sliderTheme,
      textDirection: textDirection,
      value: value,
      textScaleFactor: textScaleFactor,
      sizeWithOverflow: sizeWithOverflow,
    );

    context.canvas.drawCircle(
      center,
      enabledThumbRadius,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5
        ..color = const Color(0xff757575),
    );
  }
}
