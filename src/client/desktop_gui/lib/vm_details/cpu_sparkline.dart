import 'dart:collection';

import 'package:collection/collection.dart';
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
      spots: values.indexed.map((e) {
        final (index, value) = e;
        return FlSpot(index.toDouble(), value);
      }).toList(),
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
  var lastTotal = 0;
  var lastIdle = 0;

  @override
  Queue<double> build(String arg) {
    final usages = stateOrNull ?? Queue.of(Iterable.generate(50, (_) => 0.0));
    final cpuTimes = ref
        .watch(vmInfoProvider(arg))
        .instanceInfo
        .cpuTimes
        .split(' ')
        .skip(2)
        .take(8)
        .map(int.parse)
        .toList();

    if (cpuTimes.isEmpty) {
      return usages
        ..removeFirst()
        ..addLast(0.0);
    }

    final total = cpuTimes.sum;
    final idle = cpuTimes[3];
    final diffTotal = total - lastTotal;
    final diffIdle = idle - lastIdle;
    lastTotal = total;
    lastIdle = idle;

    final usage = (100 * (diffTotal - diffIdle) / diffTotal).roundToDouble();
    return usages
      ..removeFirst()
      ..addLast(usage);
  }
}

final cpuUsagesProvider = NotifierProvider.autoDispose
    .family<CpuUsagesNotifier, Queue<double>, String>(CpuUsagesNotifier.new);
