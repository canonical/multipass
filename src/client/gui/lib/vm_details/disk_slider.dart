import 'dart:math' as math;

import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter/services.dart';
import 'package:fpdart/fpdart.dart' hide State;
import 'package:multipass_gui/extensions.dart';

import '../dropdown.dart';
import '../ffi.dart';
import '../tooltip.dart';
import 'mapping_slider.dart';

class DiskSlider extends StatefulWidget {
  final int? initialValue;
  final bool setMinToInitial;
  final FormFieldSetter<int> onSaved;

  const DiskSlider({
    super.key,
    this.initialValue,
    required this.onSaved,
    this.setMinToInitial = false,
  });

  @override
  State<DiskSlider> createState() => _DiskSliderState();
}

class _DiskSliderState extends State<DiskSlider> {
  static final diskSize = getTotalDiskSize();

  late final min =
      widget.setMinToInitial ? widget.initialValue ?? 512.mebi : 512.mebi;
  late final max = math.max(widget.initialValue ?? 0, diskSize);
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
    final disabled = min == max;

    final textField = TextField(
      controller: controller,
      enabled: !disabled,
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
        (bytesToBytes, bytesToBytes): 'B',
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
          final clampedValue = value.clamp(min, max);
          controller.text = bytesToUnit(clampedValue).toHumanString();
        }
      }),
    );

    final sliderFormField = FormField<int>(
      key: formKey,
      initialValue: widget.initialValue,
      onSaved: (value) => widget.onSaved(value!.clamp(min, max)),
      builder: (field) {
        return MappingSlider(
          min: min,
          max: max,
          enabled: !disabled,
          value: field.value ?? min,
          onChanged: (value) {
            field.didChange(value);
            final clampedValue = value.clamp(min, max);
            controller.text = bytesToUnit(clampedValue).toHumanString();
          },
        );
      },
    );

    return Tooltip(
      message: 'Disk size cannot be decreased',
      visible: disabled,
      child: Column(children: [
        Row(children: [
          Text('Disk', style: TextStyle(fontSize: 16)),
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
      ]),
    );
  }
}
