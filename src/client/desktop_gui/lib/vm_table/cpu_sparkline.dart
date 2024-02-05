import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'cell_builders.dart';

class CpuSparkline extends ConsumerWidget {
  final String name;

  const CpuSparkline(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final values = ref.watch(cpuUsagesProvider.select((usages) {
      return usages[name] ?? const Iterable<double>.empty();
    }));

    final sparkline = LineChartBarData(
      barWidth: 1,
      belowBarData: BarAreaData(show: true, color: const Color(0xff717171)),
      color: const Color(0xff666666),
      dotData: const FlDotData(show: false),
      preventCurveOverShooting: true,
      spots: values.indexed.map((e) => FlSpot(e.$1.toDouble(), e.$2)).toList(),
    );

    return LineChart(
      duration: Duration.zero,
      LineChartData(
        borderData: FlBorderData(show: false),
        clipData: const FlClipData.all(),
        gridData: const FlGridData(show: false),
        maxX: values.length - 1,
        maxY: 110,
        minX: 0,
        minY: 0,
        lineBarsData: [sparkline],
        lineTouchData: const LineTouchData(enabled: false),
        titlesData: const FlTitlesData(show: false),
      ),
    );
  }
}
