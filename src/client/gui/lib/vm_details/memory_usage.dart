import 'package:material_ui/material_ui.dart';

import '../colors.dart';

class MemoryUsage extends StatelessWidget {
  final String used;
  final String total;

  const MemoryUsage({super.key, required this.used, required this.total});

  @override
  Widget build(BuildContext context) {
    final usedBytes = int.tryParse(used) ?? 0;
    final totalBytes = int.tryParse(total) ?? 0;
    var value = usedBytes / totalBytes;
    value = value.isFinite ? value : 0.0;
    final colors = context.colors;

    final indicator = LinearProgressIndicator(
      value: value,
      backgroundColor: colors.usageTrack,
      color: value < 0.8 ? colors.usageNormal : colors.usageHigh,
    );

    final label = Text(
      value != 0
          ? '${_formatMemory(usedBytes)} / ${_formatMemory(totalBytes)}'
          : '-',
      style: const TextStyle(fontSize: 11),
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisAlignment: MainAxisAlignment.end,
      children: [indicator, const SizedBox(height: 2), label],
    );
  }
}

String _formatMemory(int size) {
  const divider = 1024;
  const units = {
    'GiB': divider * divider * divider,
    'MiB': divider * divider,
    'KiB': divider,
  };

  for (final MapEntry(key: suffix, value: unit) in units.entries) {
    if (size >= unit) return '${(size / unit).toStringAsFixed(1)}$suffix';
  }

  return '${size}B';
}
