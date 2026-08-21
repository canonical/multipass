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

class CpuUsagesNotifier extends Notifier<Queue<double>> {
  CpuUsagesNotifier(this.arg);
  final String arg;

  var lastTotal = 0;
  var lastIdle = 0;
  var usages = Queue.of(Iterable.generate(50, (_) => 0.0));

  @override
  Queue<double> build() {
    final cpuTimes = ref
        .watch(vmInfoProvider(arg))
        .instanceInfo
        .cpuTimes
        .split(' ')
        .skip(2)
        .take(8)
        .map(int.tryParse)
        .toList();

    void append(double usage) {
      usages
        ..removeFirst()
        ..addLast(usage);
    }

    // We need at least the idle field (index 3) and every field parsed
    if (cpuTimes.length < 4 || cpuTimes.any((v) => v == null)) {
      append(0.0);
      return Queue.of(usages);
    }

    final values = cpuTimes.cast<int>();
    final total = values.sum;
    final idle = values[3];
    final diffTotal = total - lastTotal;
    final diffIdle = idle - lastIdle;
    lastTotal = total;
    lastIdle = idle;

    // Guard against a zero or negative delta
    final usage = diffTotal <= 0
        ? 0.0
        : (100 * (diffTotal - diffIdle) / diffTotal).roundToDouble();
    append(usage);
    return Queue.of(usages);
  }
}

final cpuUsagesProvider = NotifierProvider.autoDispose
    .family<CpuUsagesNotifier, Queue<double>, String>(CpuUsagesNotifier.new);
