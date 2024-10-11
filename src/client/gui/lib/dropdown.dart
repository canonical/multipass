import 'package:flutter/material.dart';

class Dropdown<T> extends StatelessWidget {
  final String? label;
  final T? value;
  final ValueChanged<T?> onChanged;
  final Map<T, String> items;
  final double width;

  const Dropdown({
    super.key,
    this.label,
    this.value,
    required this.onChanged,
    required this.items,
    this.width = 360,
  });

  @override
  Widget build(BuildContext context) {
    final dropdown = DropdownButton<T>(
      icon: const Icon(Icons.keyboard_arrow_down),
      isDense: true,
      isExpanded: true,
      focusColor: Colors.white,
      underline: const SizedBox.shrink(),
      value: value,
      onChanged: onChanged,
      items: items.entries
          .map((e) => DropdownMenuItem(value: e.key, child: Text(e.value)))
          .toList(),
    );

    final styledDropdown = SizedBox(
      width: width,
      child: InputDecorator(
        decoration: InputDecoration(
          contentPadding: const EdgeInsets.symmetric(
            horizontal: 8,
            vertical: 13,
          ),
        ),
        child: Theme(
          data: Theme.of(context).copyWith(
            splashColor: Colors.transparent,
            highlightColor: Colors.transparent,
            hoverColor: Colors.transparent,
          ),
          child: dropdown,
        ),
      ),
    );

    return Row(children: [
      if (label != null) ...[
        Expanded(child: Text(label!, style: const TextStyle(fontSize: 16))),
      ],
      styledDropdown,
    ]);
  }
}
