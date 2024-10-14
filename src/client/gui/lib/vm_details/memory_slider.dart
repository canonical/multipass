import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../dropdown.dart';
import '../extensions.dart';
import '../ffi.dart';
import 'mapping_slider.dart';

class MemorySlider extends StatefulWidget {
  final int? initialValue;
  final bool setMinToInitial;
  final FormFieldSetter<int> onSaved;
  final String label;
  final int min;
  final int max;
  final bool enabled;

  const MemorySlider({
    super.key,
    this.initialValue,
    required this.onSaved,
    this.setMinToInitial = false,
    required this.label,
    required this.min,
    required this.max,
    this.enabled = true,
  });

  @override
  State<MemorySlider> createState() => _MemorySliderState();
}

class _MemorySliderState extends State<MemorySlider> {
  var bytesToUnit = bytesToGibi;
  var unitToBytes = gibiToBytes;
  late final controller = TextEditingController(
    text: widget.initialValue.map((b) => bytesToUnit(b).toNiceString()),
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
        controller.text = bytesToUnit(currentValue).toNiceString();
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
    final parsedValue = (unitToBytes == bytesToBytes)
        ? int.tryParse(newValue.text)
        : double.tryParse(newValue.text);
    if (parsedValue == null) return oldValue;
    final convertedValue = unitToBytes(parsedValue);
    if (widget.min <= convertedValue && convertedValue <= widget.max) {
      return newValue;
    }
    return oldValue;
  }

  @override
  Widget build(BuildContext context) {
    final textField = TextField(
      controller: controller,
      enabled: widget.enabled,
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
          final clampedValue = value.clamp(widget.min, widget.max);
          controller.text = bytesToUnit(clampedValue).toNiceString();
        }
      }),
    );

    final sliderFormField = FormField<int>(
      key: formKey,
      initialValue: widget.initialValue,
      onSaved: (value) => widget.onSaved(value!.clamp(widget.min, widget.max)),
      builder: (field) {
        return MappingSlider(
          min: widget.min,
          max: widget.max,
          enabled: widget.enabled,
          value: field.value ?? widget.min,
          onChanged: (value) {
            field.didChange(value);
            final clampedValue = value.clamp(widget.min, widget.max);
            controller.text = bytesToUnit(clampedValue).toNiceString();
          },
        );
      },
    );

    return Column(children: [
      Row(children: [
        Text(widget.label, style: TextStyle(fontSize: 16)),
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
        Text(humanReadableMemory(widget.min)),
        Spacer(),
        Text(humanReadableMemory(widget.max)),
      ]),
    ]);
  }
}
