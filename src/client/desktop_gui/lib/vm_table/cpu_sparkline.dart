import 'dart:collection';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';

class CpuSparkline extends ConsumerWidget {
  final String name;

  const CpuSparkline(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final values = ref.watch(cpuUsagesProvider(name));

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

class CpuUsagesNotifier
    extends AutoDisposeFamilyNotifier<Queue<double>, String> {
  @override
  Queue<double> build(String arg) {
    final currentUsage = ref.watch(vmInfoProvider(arg).select((info) {
      return info.instanceInfo.cpuUsage;
    }));

    final usages = stateOrNull ?? Queue.of(Iterable.generate(100, (_) => 0.0));
    return usages
      ..removeFirst()
      ..addLast(currentUsage);
  }
}

final cpuUsagesProvider = NotifierProvider.autoDispose
    .family<CpuUsagesNotifier, Queue<double>, String>(CpuUsagesNotifier.new);
