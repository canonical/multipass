import 'dart:collection';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/cpu_sparkline.dart';

// A mutable provider to drive vmInfoProvider's cpuTimes field in tests.
class _CpuTimesNotifier extends Notifier<String> {
  @override
  String build() => '';

  void set(String value) => state = value;
}

final _cpuTimesProvider =
    NotifierProvider<_CpuTimesNotifier, String>(_CpuTimesNotifier.new);

ProviderContainer makeContainer() {
  final container = ProviderContainer(
    overrides: [
      vmInfoProvider('test-vm').overrideWithBuild(
        (ref, notifier) => DetailedInfoItem(
          instanceInfo: InstanceDetails(
            cpuTimes: ref.watch(_cpuTimesProvider),
          ),
        ),
      ),
    ],
  );
  addTearDown(container.dispose);
  // Keep the autoDispose provider alive for the duration of the test.
  container.listen(cpuUsagesProvider('test-vm'), (_, __) {});
  return container;
}

void main() {
  group('CpuUsagesNotifier', () {
    group('initial state with empty cpuTimes', () {
      test('queue has 50 elements', () {
        final container = makeContainer();

        final queue = container.read(cpuUsagesProvider('test-vm'));

        expect(queue.length, equals(50));
      });

      test('all 50 elements are 0.0', () {
        final container = makeContainer();

        final queue = container.read(cpuUsagesProvider('test-vm'));

        expect(queue.every((v) => v == 0.0), isTrue);
      });
    });

    group('usage calculation', () {
      // split/skip(2)/take(8) on 'h s 500 0 0 500 0 0 0 0':
      //   values = [500, 0, 0, 500, 0, 0, 0, 0]
      //   total=1000, idle=values[3]=500
      //   diffTotal=1000, diffIdle=500 → round(100*500/1000) = 50.0
      test('computes 50% usage when half of total time is idle', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 500 0 0 500 0 0 0 0');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(50.0),
        );
      });

      // split/skip(2)/take(8) on 'a b 1000 0 0 0 0 0 0 0':
      //   values = [1000, 0, 0, 0, 0, 0, 0, 0]
      //   total=1000, idle=values[3]=0
      //   diffTotal=1000, diffIdle=0 → round(100*1000/1000) = 100.0
      test('computes 100% usage when no time is idle', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('a b 1000 0 0 0 0 0 0 0');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(100.0),
        );
      });

      // split/skip(2)/take(8) on 'h s 0 0 0 1000 0 0 0 0':
      //   values = [0, 0, 0, 1000, 0, 0, 0, 0]
      //   total=1000, idle=values[3]=1000
      //   diffTotal=1000, diffIdle=1000 → round(100*0/1000) = 0.0
      test('computes 0% usage when all time is idle', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 0 0 0 1000 0 0 0 0');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(0.0),
        );
      });

      // 'cpu 100 200 300 400 500 600 700 800'
      //   split: ['cpu','100','200','300','400','500','600','700','800']
      //   skip(2)+take(8): [200, 300, 400, 500, 600, 700, 800]
      //   total=3500, idle=values[3]=500
      //   diffTotal=3500, diffIdle=500 → round(100*3000/3500) = round(85.71) = 86.0
      test('rounds fractional percentage to nearest integer', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('cpu 100 200 300 400 500 600 700 800');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(86.0),
        );
      });
    });

    group('accumulated state across builds', () {
      // First build:  'h s 500 0 0 500 0 0 0 0' → total=1000, idle=500 → lastTotal=1000, lastIdle=500
      // Second build: 'h s 1500 0 0 500 0 0 0 0' → total=2000, idle=500
      //   diffTotal=2000-1000=1000, diffIdle=500-500=0 → 100*1000/1000 = 100.0
      test('second build computes delta from previous accumulated totals', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 500 0 0 500 0 0 0 0');
        container.read(cpuUsagesProvider('test-vm')); // first build

        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 1500 0 0 500 0 0 0 0');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(100.0),
        );
      });

      // Empty cpuTimes skips the update branch, so lastTotal and lastIdle
      // remain 0 after the initial build. The next real build thus starts
      // its delta from 0.
      test('empty cpuTimes does not update accumulated totals', () {
        final container = makeContainer();
        // Initial build already used '' → lastTotal=0, lastIdle=0 unchanged.

        // Build with real data: delta is from 0.
        // [1000,0,0,0,0,0,0,0] total=1000 idle=0
        // diffTotal=1000-0=1000, diffIdle=0-0=0 → 100.0
        container
            .read(_cpuTimesProvider.notifier)
            .set('a b 1000 0 0 0 0 0 0 0');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(100.0),
        );
      });

      test('rebuilding with empty cpuTimes appends 0.0', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 500 0 0 500 0 0 0 0');
        container.read(cpuUsagesProvider('test-vm'));

        container.read(_cpuTimesProvider.notifier).set('');

        expect(
          container.read(cpuUsagesProvider('test-vm')).last,
          equals(0.0),
        );
      });
    });

    group('provider family isolation', () {
      test('different vm args have independent notifier instances', () {
        final container = ProviderContainer(
          overrides: [
            vmInfoProvider('vm-a').overrideWithBuild(
              (ref, notifier) => DetailedInfoItem(
                instanceInfo: InstanceDetails(
                  // [1000,0,0,0,0,0,0,0] total=1000, idle=0 → 100%
                  cpuTimes: 'h s 1000 0 0 0 0 0 0 0',
                ),
              ),
            ),
            vmInfoProvider('vm-b').overrideWithBuild(
              (ref, notifier) => DetailedInfoItem(
                instanceInfo: InstanceDetails(
                  // [500,0,0,500,0,0,0,0] total=1000, idle=500 → 50%
                  cpuTimes: 'h s 500 0 0 500 0 0 0 0',
                ),
              ),
            ),
          ],
        );
        addTearDown(container.dispose);
        container.listen(cpuUsagesProvider('vm-a'), (_, __) {});
        container.listen(cpuUsagesProvider('vm-b'), (_, __) {});

        expect(
          container.read(cpuUsagesProvider('vm-a')).last,
          equals(100.0),
        );
        expect(
          container.read(cpuUsagesProvider('vm-b')).last,
          equals(50.0),
        );
      });
    });
  });
}
