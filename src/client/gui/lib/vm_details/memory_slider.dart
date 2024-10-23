import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../dropdown.dart';
import '../extensions.dart';
import '../ffi.dart';
import 'mapping_slider.dart';

class MemorySlider extends StatefulWidget {
  final int? initialValue;
  final FormFieldSetter<int> onSaved;
  final String label;
  final int min;
  final int max;
  final bool enabled;
  final int sysMax;

  const MemorySlider({
    super.key,
    this.initialValue,
    required this.onSaved,
    required this.label,
    required this.min,
    required this.max,
    this.enabled = true,
    required this.sysMax,
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
      if (!focusNode.hasFocus && currentValue != null) {
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
    return parsedValue == null ? oldValue : newValue;
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
        if (parsedValue == null) return;
        final convertedValue = unitToBytes(parsedValue).toInt();
        formKey.currentState?.didChange(
          convertedValue.clamp(widget.min, widget.max),
        );
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
        return Column(children: [
          MappingSlider(
            min: widget.min,
            max: widget.max,
            enabled: widget.enabled,
            value: field.value ?? widget.min,
            onChanged: (value) {
              field.didChange(value);
              final clampedValue = value.clamp(widget.min, widget.max);
              controller.text = bytesToUnit(clampedValue).toNiceString();
            },
          ),
          const SizedBox(height: 5),
          Row(children: [
            Text(humanReadableMemory(widget.min)),
            Spacer(),
            Text(humanReadableMemory(widget.max)),
          ]),
          if ((field.value ?? widget.min) > widget.sysMax) ...[
            const SizedBox(height: 25),
            Row(children: [
              const Icon(Icons.warning_rounded, color: Color(0xffCC7900)),
              const SizedBox(width: 5),
              Text(
                'Over-provisioning of ${widget.label.toLowerCase()}',
                style: const TextStyle(fontSize: 16),
              ),
            ]),
          ],
        ]);
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
    ]);
  }
}
