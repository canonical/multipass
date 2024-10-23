import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:system_info2/system_info2.dart';

class CpusSlider extends StatefulWidget {
  final int? initialValue;
  final FormFieldSetter<int> onSaved;

  const CpusSlider({
    super.key,
    this.initialValue,
    required this.onSaved,
  });

  @override
  State<CpusSlider> createState() => _CpusSliderState();
}

class _CpusSliderState extends State<CpusSlider> {
  static final cores = SysInfo.cores.length;

  final min = 1;
  late final max = math.max(widget.initialValue ?? 0, cores);
  late final controller = TextEditingController(
    text: widget.initialValue?.toString(),
  );
  final focusNode = FocusNode();
  final formKey = GlobalKey<FormFieldState<int>>();

  @override
  void initState() {
    super.initState();
    focusNode.addListener(() {
      final currentValue = formKey.currentState?.value;
      if (!focusNode.hasFocus && currentValue != null) {
        controller.text = currentValue.toString();
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
    return int.tryParse(newValue.text) == null ? oldValue : newValue;
  }

  @override
  Widget build(BuildContext context) {
    final textField = TextField(
      controller: controller,
      focusNode: focusNode,
      textAlign: TextAlign.right,
      inputFormatters: [TextInputFormatter.withFunction(formatFunction)],
      onChanged: (value) {
        final parsedValue = int.tryParse(value);
        if (parsedValue == null) return;
        formKey.currentState?.didChange(parsedValue.clamp(min, max));
      },
    );

    final sliderFormField = FormField<int>(
      key: formKey,
      initialValue: widget.initialValue,
      onSaved: widget.onSaved,
      builder: (field) {
        return Column(children: [
          Slider(
            min: min.toDouble(),
            max: max.toDouble(),
            divisions: max - min,
            value: (field.value ?? min).toDouble(),
            onChanged: (value) {
              final intValue = value.toInt();
              field.didChange(intValue);
              controller.text = intValue.toString();
            },
          ),
          const SizedBox(height: 5),
          Row(children: [Text('$min'), Spacer(), Text('$max')]),
          if ((field.value ?? min) > cores) ...[
            const SizedBox(height: 25),
            const Row(children: [
              Icon(Icons.warning_rounded, color: Color(0xffCC7900)),
              SizedBox(width: 5),
              Text(
                'Over-provisioning of cores',
                style: TextStyle(fontSize: 16),
              ),
            ]),
          ],
        ]);
      },
    );

    return Column(children: [
      Row(children: [
        Text('CPUs', style: TextStyle(fontSize: 16)),
        const Spacer(),
        SizedBox(width: 65, child: textField),
      ]),
      const SizedBox(height: 25),
      sliderFormField,
    ]);
  }
}
