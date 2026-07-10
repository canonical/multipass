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
    test('starts with a flat history of zero usage', () {
      final container = makeContainer();

      final history = container.read(cpuUsagesProvider('test-vm'));

      expect(history, isNotEmpty);
      expect(history.every((v) => v == 0.0), isTrue);
    });

    test('keeps a fixed-length history as new samples arrive', () {
      final container = makeContainer();
      final initialLength = container.read(cpuUsagesProvider('test-vm')).length;

      container.read(_cpuTimesProvider.notifier).set('h s 500 0 0 500 0 0 0 0');

      expect(
        container.read(cpuUsagesProvider('test-vm')).length,
        equals(initialLength),
      );
    });

    group('usage calculation', () {
      test('reports 0% usage when the CPU was fully idle', () {
        final container = makeContainer();

        // A single interval where all elapsed time is idle.
        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 0 0 0 1000 0 0 0 0');

        expect(container.read(cpuUsagesProvider('test-vm')).last, equals(0.0));
      });

      test('reports 100% usage when the CPU was fully busy', () {
        final container = makeContainer();

        // All elapsed time is non-idle.
        container
            .read(_cpuTimesProvider.notifier)
            .set('a b 1000 0 0 0 0 0 0 0');

        expect(
            container.read(cpuUsagesProvider('test-vm')).last, equals(100.0));
      });

      test('reports 50% usage when the CPU was busy for half the interval', () {
        final container = makeContainer();

        // Half of the elapsed time is idle, half is busy.
        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 500 0 0 500 0 0 0 0');

        expect(container.read(cpuUsagesProvider('test-vm')).last, equals(50.0));
      });

      test('rounds the usage percentage to a whole number', () {
        final container = makeContainer();

        // 3000 busy out of 3500 total is 85.71%, expected to round to 86.
        container
            .read(_cpuTimesProvider.notifier)
            .set('cpu 100 200 300 400 500 600 700 800');

        expect(container.read(cpuUsagesProvider('test-vm')).last, equals(86.0));
      });
    });

    group('deltas between samples', () {
      test('computes usage from the change since the previous sample', () {
        final container = makeContainer();

        // First sample establishes the baseline counters.
        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 500 0 0 500 0 0 0 0');
        container.read(cpuUsagesProvider('test-vm'));

        // Second sample: 1000 more total, none of it idle → 100% for this
        // interval, even though the cumulative counters are not fully busy.
        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 1500 0 0 500 0 0 0 0');

        expect(
            container.read(cpuUsagesProvider('test-vm')).last, equals(100.0));
      });

      test('records 0% usage for an empty sample', () {
        final container = makeContainer();

        container
            .read(_cpuTimesProvider.notifier)
            .set('h s 500 0 0 500 0 0 0 0');
        container.read(cpuUsagesProvider('test-vm'));

        container.read(_cpuTimesProvider.notifier).set('');

        expect(container.read(cpuUsagesProvider('test-vm')).last, equals(0.0));
      });

      test('ignores empty samples when computing subsequent deltas', () {
        final container = makeContainer();
        // The initial build already saw an empty sample, so the baseline
        // counters remain at zero and the next real sample is a full delta.

        container
            .read(_cpuTimesProvider.notifier)
            .set('a b 1000 0 0 0 0 0 0 0');

        expect(
            container.read(cpuUsagesProvider('test-vm')).last, equals(100.0));
      });
    });

    test('tracks usage independently for each VM', () {
      final container = ProviderContainer(
        overrides: [
          vmInfoProvider('vm-a').overrideWithBuild(
            (ref, notifier) => DetailedInfoItem(
              instanceInfo: InstanceDetails(
                cpuTimes: 'h s 1000 0 0 0 0 0 0 0', // fully busy → 100%
              ),
            ),
          ),
          vmInfoProvider('vm-b').overrideWithBuild(
            (ref, notifier) => DetailedInfoItem(
              instanceInfo: InstanceDetails(
                cpuTimes: 'h s 500 0 0 500 0 0 0 0', // half busy → 50%
              ),
            ),
          ),
        ],
      );
      addTearDown(container.dispose);
      container.listen(cpuUsagesProvider('vm-a'), (_, __) {});
      container.listen(cpuUsagesProvider('vm-b'), (_, __) {});

      expect(container.read(cpuUsagesProvider('vm-a')).last, equals(100.0));
      expect(container.read(cpuUsagesProvider('vm-b')).last, equals(50.0));
    });
  });
}
