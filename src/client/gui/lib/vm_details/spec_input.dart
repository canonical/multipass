import 'package:material_ui/material_ui.dart';
import 'package:flutter/services.dart';

import '../colors.dart';

class SpecInput extends StatelessWidget {
  final bool autofocus;
  final bool enabled;
  final String? helper;
  final String? hint;
  final String? initialValue;
  final String? label;
  final FormFieldSetter<String>? onSaved;
  final FormFieldValidator<String>? validator;
  final List<TextInputFormatter>? inputFormatters;
  final double width;
  final TextEditingController? controller;
  final FocusNode? focusNode;

  const SpecInput({
    super.key,
    this.autofocus = false,
    this.enabled = true,
    this.helper,
    this.hint,
    this.initialValue,
    this.label,
    this.onSaved,
    this.validator,
    this.inputFormatters,
    this.width = 170,
    this.controller,
    this.focusNode,
  });

  @override
  Widget build(BuildContext context) {
    final textStyle = TextStyle(color: context.colors.textPrimary);
    return SizedBox(
      width: width,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (label != null) ...[
            Text(label!, style: textStyle.copyWith(fontSize: 16)),
            const SizedBox(height: 8),
          ],
          TextFormField(
            autofocus: autofocus,
            controller: controller,
            decoration: InputDecoration(
              hintText: hint,
              helperText: helper,
              helperMaxLines: 3,
              helperStyle: textStyle,
              disabledBorder: OutlineInputBorder(
                borderSide: BorderSide(color: context.colors.disabledBorder),
                borderRadius: BorderRadius.zero,
              ),
            ),
            enabled: enabled,
            focusNode: focusNode,
            initialValue: initialValue,
            inputFormatters: inputFormatters,
            onSaved: onSaved,
            style: textStyle,
            validator: validator,
          ),
        ],
      ),
    );
  }
}
