import 'package:flutter/cupertino.dart' show CupertinoColors;
import 'package:flutter/material.dart';

const settingLabelStyle = TextStyle(fontSize: 16);

class SettingsSectionTitle extends StatelessWidget {
  const SettingsSectionTitle(this.text, {super.key});

  final String text;

  @override
  Widget build(BuildContext context) => Text(
        text,
        style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
      );
}

class FixedOptionsSetting<T> extends StatelessWidget {
  const FixedOptionsSetting({
    super.key,
    required this.label,
    required this.options,
    required this.value,
    required this.onChanged,
    this.subtitle,
  });

  final String label;
  final Map<T, String> options;
  final T? value;
  final ValueChanged<T> onChanged;
  final Widget? subtitle;

  @override
  Widget build(BuildContext context) {
    const idleGrey = Color(0xfff2f2f2);
    final hoverGrey = Color.alphaBlend(Theme.of(context).hoverColor, idleGrey);

    return Row(
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(label, style: settingLabelStyle),
              if (subtitle != null) ...[
                const SizedBox(height: 4),
                subtitle!,
              ],
            ],
          ),
        ),
        const SizedBox(width: 20),
        SegmentedButton<T>(
          emptySelectionAllowed: true,
          showSelectedIcon: false,
          style: ButtonStyle(
            backgroundColor: WidgetStateProperty.resolveWith((states) {
              if (states.contains(WidgetState.selected)) {
                return CupertinoColors.activeBlue;
              }
              if (states.contains(WidgetState.hovered)) {
                return hoverGrey;
              }
              return idleGrey;
            }),
            foregroundColor: WidgetStateProperty.resolveWith((states) {
              return states.contains(WidgetState.selected)
                  ? Colors.white
                  : Colors.black87;
            }),
            padding: WidgetStateProperty.all(const EdgeInsets.all(16)),
            overlayColor: WidgetStateProperty.all(Colors.transparent),
            mouseCursor: WidgetStateProperty.all(SystemMouseCursors.click),
          ),
          segments: [
            for (final entry in options.entries)
              ButtonSegment(value: entry.key, label: Text(entry.value)),
          ],
          selected: {if (value != null) value as T},
          onSelectionChanged: (selection) {
            if (selection.isEmpty) return;
            onChanged(selection.first);
          },
        ),
      ],
    );
  }
}
