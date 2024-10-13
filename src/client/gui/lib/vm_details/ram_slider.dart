import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:fpdart/fpdart.dart' hide State;
import 'package:system_info2/system_info2.dart';

import '../dropdown.dart';
import '../extensions.dart';
import '../ffi.dart';
import 'mapping_slider.dart';

class RamSlider extends StatefulWidget {
  final int? initialValue;
  final FormFieldSetter<int> onSaved;

  const RamSlider({
    super.key,
    this.initialValue,
    required this.onSaved,
  });

  @override
  State<RamSlider> createState() => _RamSliderState();
}

class _RamSliderState extends State<RamSlider> {
  static final ram = SysInfo.getTotalPhysicalMemory();

  final min = 512.mebi;
  late final max = math.max(widget.initialValue ?? 0, ram);
  var bytesToUnit = bytesToGibi;
  var unitToBytes = gibiToBytes;
  late final controller = TextEditingController(
    text: widget.initialValue.map((b) => bytesToUnit(b).toHumanString()),
  );
  final focusNode = FocusNode();
  final formKey = GlobalKey<FormFieldState<int>>();

  @override
  void initState() {
    super.initState();
    focusNode.addListener(() {
      final currentValue = formKey.currentState?.value;
      if (!focusNode.hasFocus &&
          controller.text.isEmpty &&
          currentValue != null) {
        controller.text = bytesToUnit(currentValue).toHumanString();
      }
    });
  }

  @override
  void dispose() {
    controller.dispose();
    focusNode.dispose();
    super.dispose();
  }

  TextEditingValue formatFunction(
    TextEditingValue oldValue,
    TextEditingValue newValue,
  ) {
    if (newValue.text.isEmpty) return newValue;
    final parsedValue = (unitToBytes == identity<num>)
        ? int.tryParse(newValue.text)
        : double.tryParse(newValue.text);
    if (parsedValue == null) return oldValue;
    final convertedValue = unitToBytes(parsedValue);
    if (min <= convertedValue && convertedValue <= max) return newValue;
    return oldValue;
  }

  @override
  Widget build(BuildContext context) {
    final textField = TextField(
      controller: controller,
      focusNode: focusNode,
      textAlign: TextAlign.right,
      inputFormatters: [TextInputFormatter.withFunction(formatFunction)],
      onChanged: (value) {
        final parsedValue = double.tryParse(value);
        if (parsedValue != null) {
          formKey.currentState?.didChange(unitToBytes(parsedValue).toInt());
        }
      },
    );

    final unitDropdown = Dropdown<(num Function(num), num Function(num))>(
      value: (bytesToUnit, unitToBytes),
      width: 70,
      items: {
        (identity<num>, identity<num>): 'B',
        (bytesToKibi, kibiToBytes): 'KiB',
        (bytesToMebi, mebiToBytes): 'MiB',
        (bytesToGibi, gibiToBytes): 'GiB',
      },
      onChanged: (convertors) => setState(() {
        final (fromBytes, toBytes) = convertors!;
        bytesToUnit = fromBytes;
        unitToBytes = toBytes;
        final value = formKey.currentState?.value;
        if (value != null) {
          final clampedValue = math.min(value, max);
          controller.text = bytesToUnit(clampedValue).toHumanString();
        }
      }),
    );

    final sliderFormField = FormField<int>(
      key: formKey,
      initialValue: widget.initialValue,
      onSaved: (value) => widget.onSaved(math.min(value!, max)),
      builder: (field) {
        return MappingSlider(
          min: min,
          max: max,
          value: field.value ?? min,
          onChanged: (value) {
            field.didChange(value);
            final clampedValue = math.min(value, max);
            controller.text = bytesToUnit(clampedValue).toHumanString();
          },
        );
      },
    );

    return Column(children: [
      Row(children: [
        Text('Memory', style: TextStyle(fontSize: 16)),
        Spacer(),
        ConstrainedBox(
          constraints: BoxConstraints(minWidth: 65),
          child: IntrinsicWidth(child: textField),
        ),
        SizedBox(width: 8),
        unitDropdown,
      ]),
      const SizedBox(height: 25),
      sliderFormField,
      const SizedBox(height: 5),
      Row(children: [
        Text(humanReadableMemory(min)),
        Spacer(),
        Text(humanReadableMemory(max)),
      ]),
    ]);
  }
}
