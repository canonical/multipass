import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

class SpecInput extends StatelessWidget {
  final bool autofocus;
  final String? helper;
  final String? hint;
  final String? initialValue;
  final String? label;
  final FormFieldSetter<String> onSaved;
  final FormFieldValidator<String>? validator;
  final List<TextInputFormatter>? inputFormatters;
  final double width;

  const SpecInput({
    super.key,
    this.autofocus = false,
    this.helper,
    this.hint,
    this.initialValue,
    this.label,
    required this.onSaved,
    this.validator,
    this.inputFormatters,
    this.width = 170,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: width,
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        if (label != null) ...[
          Text(label!, style: const TextStyle(fontSize: 16)),
          const SizedBox(height: 8),
        ],
        TextFormField(
          autofocus: autofocus,
          decoration: InputDecoration(
            hintText: hint,
            helperText: helper,
            helperMaxLines: 3,
            helperStyle: const TextStyle(color: Colors.black),
          ),
          initialValue: initialValue,
          onSaved: onSaved,
          validator: validator,
          inputFormatters: inputFormatters,
        ),
      ]),
    );
  }
}
