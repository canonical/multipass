import 'package:flutter/material.dart';

class MemoryUsage extends StatelessWidget {
  final String used;
  final String total;

  const MemoryUsage({super.key, required this.used, required this.total});

  static const normalColor = Color(0xff0066cc);
  static const almostFullColor = Color(0xffEC6C04);
  static const backgroundColor = Color(0x3d006ada);

  @override
  Widget build(BuildContext context) {
    var value = (double.tryParse(used) ?? 0) / (double.tryParse(total) ?? 1);
    value = value.isFinite ? value : 0.0;

    final indicator = LinearProgressIndicator(
      value: value,
      backgroundColor: backgroundColor,
      color: value < 0.8 ? normalColor : almostFullColor,
    );

    final label = Text(
      value != 0 ? '${_formatMemory(used)} / ${_formatMemory(total)}' : '-',
      style: const TextStyle(fontSize: 11),
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisAlignment: MainAxisAlignment.end,
      children: [indicator, const SizedBox(height: 2), label],
    );
  }
}

String _formatMemory(String data) {
  const divider = 1024;
  const units = {
    'GiB': divider * divider * divider,
    'MiB': divider * divider,
    'KiB': divider,
  };

  final size = int.parse(data);
  for (final MapEntry(key: suffix, value: unit) in units.entries) {
    if (size >= unit) return '${(size / unit).toStringAsFixed(1)}$suffix';
  }

  return '${size}B';
}
