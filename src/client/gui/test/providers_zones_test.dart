import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_details/vm_status_icon.dart';

typedef PollingData = ({List<VmInfo> info, List<Zone> zones});

void main() {
  ProviderContainer buildContainer(Stream<PollingData> stream) {
    final container = ProviderContainer(
      // Disable Riverpod's automatic retry-on-error. On a stream error the
      // StreamProvider would otherwise re-invoke its create function and
      // re-listen the (single-subscription) override stream, throwing
      // "Bad state: Stream has already been listened to." Returning null keeps
      // the error terminal so `pollingProvider.future` completes with it.
      retry: (_, __) => null,
      overrides: [
        pollingProvider.overrideWith((ref) => stream),
      ],
    );
    // Keep the StreamProvider actively subscribed so its stream is listened to;
    // otherwise reading `.future` never completes and the test times out.
    container.listen(pollingProvider, (_, __) {});
    return container;
  }

  ResponseStream<ZonesReply> buildZonesResponse({
    Iterable<Zone> zones = const [],
    GrpcError? error,
  }) {
    final responseStream = error == null
        ? Stream<ZonesReply>.value(ZonesReply(zones: zones))
        : Stream<ZonesReply>.error(error);

    return ResponseStream(
      _FakeClientCall<ZonesReply>(responseStream),
    );
  }

  Widget buildStatusIcon() {
    return ProviderScope(
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: const Scaffold(
          body: VmStatusIcon(Status.UNAVAILABLE, isLaunching: false),
        ),
      ),
    );
  }

  test('icons map includes Status.UNAVAILABLE', () {
    final icon = icons[Status.UNAVAILABLE];

    expect(icon, isNotNull);
    expect(icon!.icon, Icons.circle);
    expect(icon.color, const Color(0xff757575));
    expect(icon.size, 10);
  });

  group('zonesProvider', () {
    test('returns the zones from polling data', () async {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      final zone = Zone(name: 'zone-a', available: true);
      controller.add((info: const <VmInfo>[], zones: [zone]));

      await container.read(pollingProvider.future);

      final zones = container.read(zonesProvider);

      expect(zones, hasLength(1));
      expect(zones.single.name, 'zone-a');
      expect(zones.single.available, isTrue);
    });

    test('returns an empty list while loading', () {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      expect(container.read(zonesProvider), isEmpty);
    });

    test('returns an empty list when polling errors', () async {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      controller.addError(Exception('polling failed'));

      await expectLater(
        container.read(pollingProvider.future),
        throwsA(isA<Exception>()),
      );

      expect(container.read(zonesProvider), isEmpty);
    });
  });

  group('azSupportedProvider', () {
    test('returns true when polling data has zones', () async {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      controller.add((
        info: const <VmInfo>[],
        zones: [Zone(name: 'zone-a', available: true)],
      ));

      await container.read(pollingProvider.future);

      expect(container.read(azSupportedProvider), isTrue);
    });

    test('returns false when polling data has no zones', () async {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      controller.add((info: const <VmInfo>[], zones: const <Zone>[]));

      await container.read(pollingProvider.future);

      expect(container.read(azSupportedProvider), isFalse);
    });

    test('returns true while loading', () {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      expect(container.read(azSupportedProvider), isTrue);
    });

    test('returns true when polling errors', () async {
      final controller = StreamController<PollingData>();
      final container = buildContainer(controller.stream);
      addTearDown(controller.close);
      addTearDown(container.dispose);

      controller.addError(Exception('polling failed'));

      await expectLater(
        container.read(pollingProvider.future),
        throwsA(isA<Exception>()),
      );

      expect(container.read(azSupportedProvider), isTrue);
    });
  });

  group('GrpcClient.zones', () {
    test('returns an empty list for FAILED_PRECONDITION not supported',
        () async {
      final client = GrpcClient(
        _FakeRpcClient(
          buildZonesResponse(
            error: const GrpcError.failedPrecondition(
              'Feature is not supported in this backend',
            ),
          ),
        ),
      );

      await expectLater(client.zones(), completion(isEmpty));
    });

    test('rethrows other GrpcErrors', () async {
      final client = GrpcClient(
        _FakeRpcClient(
          buildZonesResponse(
            error: const GrpcError.permissionDenied('nope'),
          ),
        ),
      );

      await expectLater(
        client.zones(),
        throwsA(
          isA<GrpcError>().having(
            (error) => error.code,
            'code',
            StatusCode.permissionDenied,
          ),
        ),
      );
    });
  });

  testWidgets('VmStatusIcon uses the UNAVAILABLE icon', (tester) async {
    await tester.pumpWidget(buildStatusIcon());
    await tester.pumpAndSettle();

    final icon = tester.widget<Icon>(find.byType(Icon));

    expect(icon.icon, Icons.circle);
    expect(icon.color, const Color(0xff757575));
    expect(icon.size, 10);
  });
}

class _FakeRpcClient extends RpcClient {
  _FakeRpcClient(this._zonesResponse)
      : super(
          ClientChannel(
            'localhost',
            options: const ChannelOptions(
              credentials: ChannelCredentials.insecure(),
            ),
          ),
        );

  final ResponseStream<ZonesReply> _zonesResponse;

  @override
  ResponseStream<ZonesReply> zones(
    Stream<ZonesRequest> request, {
    CallOptions? options,
  }) {
    return _zonesResponse;
  }
}

class _FakeClientCall<R> extends ClientCall<dynamic, R> {
  _FakeClientCall(this._response)
      : super(
          ClientMethod<dynamic, R>(
            '/multipass.test/Fake',
            (value) => const <int>[],
            (value) => throw UnimplementedError(),
          ),
          const Stream<dynamic>.empty(),
          CallOptions(),
        );

  final Stream<R> _response;

  @override
  Stream<R> get response => _response;

  @override
  Future<Map<String, String>> get headers async => const {};

  @override
  Future<Map<String, String>> get trailers async => const {};

  @override
  Future<void> cancel() async {}
}
