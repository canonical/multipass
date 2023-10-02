import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';

class CpuSparkline extends StatelessWidget {
  final Iterable<double> values;

  const CpuSparkline(this.values, {super.key});

  @override
  Widget build(BuildContext context) {
    return LineChart(
      LineChartData(
        minY: 0,
        maxY: 110,
        minX: 0,
        maxX: values.length - 1,
        lineTouchData: const LineTouchData(enabled: false),
        clipData: const FlClipData.all(),
        gridData: const FlGridData(show: false),
        borderData: FlBorderData(show: false),
        lineBarsData: [_sparkline(values)],
        titlesData: const FlTitlesData(show: false),
      ),
      duration: Duration.zero,
    );
  }
}

LineChartBarData _sparkline(Iterable<double> values) {
  return LineChartBarData(
    spots: values.indexed.map((e) => FlSpot(e.$1.toDouble(), e.$2)).toList(),
    dotData: const FlDotData(show: false),
    color: const Color(0xff666666),
    barWidth: 1,
    belowBarData: BarAreaData(show: true, color: const Color(0xff717171)),
    preventCurveOverShooting: true,
  );
}
