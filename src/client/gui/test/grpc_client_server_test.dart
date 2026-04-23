import 'dart:async';
import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:logger/logger.dart';
import 'package:multipass_gui/logger.dart' show logger;
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/update_available.dart';

// Stub Grpc service
class _StubService extends RpcServiceBase {
  static Never _unimplemented() => throw GrpcError.unimplemented('not set up');

  Stream<StartReply> Function(StartRequest) onStart = (_) => _unimplemented();
  Stream<StopReply> Function(StopRequest) onStop = (_) => _unimplemented();
  Stream<SuspendReply> Function(SuspendRequest) onSuspend =
      (_) => _unimplemented();
  Stream<RestartReply> Function(RestartRequest) onRestart =
      (_) => _unimplemented();
  Stream<DeleteReply> Function(DeleteRequest) onDelet = (_) => _unimplemented();
  Stream<RecoverReply> Function(RecoverRequest) onRecover =
      (_) => _unimplemented();
  Stream<FindReply> Function(FindRequest) onFind = (_) => _unimplemented();
  Stream<NetworksReply> Function(NetworksRequest) onNetworks =
      (_) => _unimplemented();
  Stream<VersionReply> Function(VersionRequest) onVersion =
      (_) => _unimplemented();
  Stream<InfoReply> Function(InfoRequest) onInfo = (_) => _unimplemented();
  Stream<GetReply> Function(GetRequest) onGet = (_) => _unimplemented();
  Stream<SetReply> Function(SetRequest) onSet = (_) => _unimplemented();
  Stream<SSHInfoReply> Function(SSHInfoRequest) onSshInfo =
      (_) => _unimplemented();
  Stream<DaemonInfoReply> Function(DaemonInfoRequest) onDaemonInfo =
      (_) => _unimplemented();

  @override
  Stream<StartReply> start(ServiceCall c, Stream<StartRequest> req) async* {
    yield* onStart(await req.first);
  }

  @override
  Stream<StopReply> stop(ServiceCall c, Stream<StopRequest> req) async* {
    yield* onStop(await req.first);
  }

  @override
  Stream<SuspendReply> suspend(
      ServiceCall c, Stream<SuspendRequest> req) async* {
    yield* onSuspend(await req.first);
  }

  @override
  Stream<RestartReply> restart(
      ServiceCall c, Stream<RestartRequest> req) async* {
    yield* onRestart(await req.first);
  }

  @override
  Stream<DeleteReply> delet(ServiceCall c, Stream<DeleteRequest> req) async* {
    yield* onDelet(await req.first);
  }

  @override
  Stream<RecoverReply> recover(
      ServiceCall c, Stream<RecoverRequest> req) async* {
    yield* onRecover(await req.first);
  }

  @override
  Stream<FindReply> find(ServiceCall c, Stream<FindRequest> req) async* {
    yield* onFind(await req.first);
  }

  @override
  Stream<NetworksReply> networks(
      ServiceCall c, Stream<NetworksRequest> req) async* {
    yield* onNetworks(await req.first);
  }

  @override
  Stream<VersionReply> version(
      ServiceCall c, Stream<VersionRequest> req) async* {
    yield* onVersion(await req.first);
  }

  @override
  Stream<InfoReply> info(ServiceCall c, Stream<InfoRequest> req) async* {
    yield* onInfo(await req.first);
  }

  @override
  Stream<GetReply> get(ServiceCall c, Stream<GetRequest> req) async* {
    yield* onGet(await req.first);
  }

  @override
  Stream<SetReply> set(ServiceCall c, Stream<SetRequest> req) async* {
    yield* onSet(await req.first);
  }

  @override
  Stream<SSHInfoReply> ssh_info(
      ServiceCall c, Stream<SSHInfoRequest> req) async* {
    yield* onSshInfo(await req.first);
  }

  @override
  Stream<DaemonInfoReply> daemon_info(
      ServiceCall c, Stream<DaemonInfoRequest> req) async* {
    yield* onDaemonInfo(await req.first);
  }

  @override
  Stream<LaunchReply> create(ServiceCall c, Stream<LaunchRequest> r) =>
      _unimplemented();
  @override
  Stream<LaunchReply> launch(ServiceCall c, Stream<LaunchRequest> r) =>
      _unimplemented();
  @override
  Stream<PurgeReply> purge(ServiceCall c, Stream<PurgeRequest> r) =>
      _unimplemented();
  @override
  Stream<ListReply> list(ServiceCall c, Stream<ListRequest> r) =>
      _unimplemented();
  @override
  Stream<MountReply> mount(ServiceCall c, Stream<MountRequest> r) =>
      _unimplemented();
  @override
  Future<PingReply> ping(ServiceCall c, PingRequest r) => _unimplemented();
  @override
  Stream<UmountReply> umount(ServiceCall c, Stream<UmountRequest> r) =>
      _unimplemented();
  @override
  Stream<KeysReply> keys(ServiceCall c, Stream<KeysRequest> r) =>
      _unimplemented();
  @override
  Stream<AuthenticateReply> authenticate(
          ServiceCall c, Stream<AuthenticateRequest> r) =>
      _unimplemented();
  @override
  Stream<SnapshotReply> snapshot(ServiceCall c, Stream<SnapshotRequest> r) =>
      _unimplemented();
  @override
  Stream<RestoreReply> restore(ServiceCall c, Stream<RestoreRequest> r) =>
      _unimplemented();
  @override
  Stream<CloneReply> clone(ServiceCall c, Stream<CloneRequest> r) =>
      _unimplemented();
  @override
  Stream<WaitReadyReply> wait_ready(
          ServiceCall c, Stream<WaitReadyRequest> r) =>
      _unimplemented();
}

late _StubService _stub;
late GrpcClient _client;
late Server _server;
late ClientChannel _channel;

Future<void> _startServer() async {
  _stub = _StubService();
  _server = Server.create(services: [_stub]);
  await _server.serve(address: InternetAddress.loopbackIPv4, port: 0);

  _channel = ClientChannel(
    'localhost',
    port: _server.port!,
    options: const ChannelOptions(credentials: ChannelCredentials.insecure()),
  );
  _client = GrpcClient(RpcClient(_channel));
}

Future<void> _stopServer() async {
  await _channel.shutdown();
  await _server.shutdown();
}

void main() {
  setUpAll(() {
    logger = Logger(filter: ProductionFilter(), output: MemoryOutput());

    providerContainer = ProviderContainer();
    providerContainer.listen(updateProvider, (_, __) {});
  });

  setUp(_startServer);
  tearDown(_stopServer);
  tearDown(() => providerContainer.invalidate(updateProvider));
  tearDownAll(() => providerContainer.dispose());

  group('GrpcClient.start', () {
    test('sends the vm names in the StartRequest', () async {
      List<String>? captured;
      _stub.onStart = (req) {
        captured = req.instanceNames.instanceName.toList();
        return Stream.value(StartReply());
      };
      await _client.start(['vm-a', 'vm-b']);
      expect(captured, equals(['vm-a', 'vm-b']));
    });

    test('calls checkForUpdate and sets updateProvider', () async {
      final info = UpdateInfo()..version = '2.0.0';
      _stub.onStart = (_) => Stream.value(StartReply()..updateInfo = info);
      await _client.start(['vm-a']);
      expect(providerContainer.read(updateProvider).version, equals('2.0.0'));
    });
  });

  group('GrpcClient.stop', () {
    test('sends the vm names in the StopRequest', () async {
      List<String>? captured;
      _stub.onStop = (req) {
        captured = req.instanceNames.instanceName.toList();
        return Stream.value(StopReply());
      };
      await _client.stop(['my-vm']);
      expect(captured, equals(['my-vm']));
    });
  });

  group('GrpcClient.suspend', () {
    test('sends the vm names in the SuspendRequest', () async {
      List<String>? captured;
      _stub.onSuspend = (req) {
        captured = req.instanceNames.instanceName.toList();
        return Stream.value(SuspendReply());
      };
      await _client.suspend(['my-vm']);
      expect(captured, equals(['my-vm']));
    });
  });

  group('GrpcClient.restart', () {
    test('sends the vm names in the RestartRequest', () async {
      List<String>? captured;
      _stub.onRestart = (req) {
        captured = req.instanceNames.instanceName.toList();
        return Stream.value(RestartReply());
      };
      await _client.restart(['my-vm']);
      expect(captured, equals(['my-vm']));
    });

    test('calls checkForUpdate and sets updateProvider', () async {
      final info = UpdateInfo()..version = '3.0.0';
      _stub.onRestart = (_) => Stream.value(RestartReply()..updateInfo = info);
      await _client.restart(['my-vm']);
      expect(providerContainer.read(updateProvider).version, equals('3.0.0'));
    });
  });

  group('GrpcClient.delete', () {
    test('sends a non-purge DeleteRequest with the instance names', () async {
      DeleteRequest? captured;
      _stub.onDelet = (req) {
        captured = req;
        return Stream.value(DeleteReply());
      };
      await _client.delete(['vm-a']);
      expect(captured?.purge, isFalse);
      expect(
          captured?.instanceSnapshotPairs.map((p) => p.instanceName).toList(),
          equals(['vm-a']));
    });
  });

  group('GrpcClient.purge', () {
    test('sends a purge=true DeleteRequest', () async {
      DeleteRequest? captured;
      _stub.onDelet = (req) {
        captured = req;
        return Stream.value(DeleteReply());
      };
      await _client.purge(['vm-a']);
      expect(captured?.purge, isTrue);
    });
  });

  group('GrpcClient.recover', () {
    test('sends the vm names in the RecoverRequest', () async {
      List<String>? captured;
      _stub.onRecover = (req) {
        captured = req.instanceNames.instanceName.toList();
        return Stream.value(RecoverReply());
      };
      await _client.recover(['my-vm']);
      expect(captured, equals(['my-vm']));
    });
  });

  group('GrpcClient.find', () {
    test('returns the FindReply from the server', () async {
      final imageInfo = FindReply_ImageInfo()..aliases.add('jammy');
      _stub.onFind =
          (_) => Stream.value(FindReply()..imagesInfo.add(imageInfo));
      final reply = await _client.find();
      expect(reply.imagesInfo.first.aliases, contains('jammy'));
    });
  });

  group('GrpcClient.networks', () {
    test('returns the list of interfaces', () async {
      _stub.onNetworks = (_) => Stream.value(
            NetworksReply()..interfaces.add(NetInterface()..name = 'eth0'),
          );
      final interfaces = await _client.networks();
      expect(interfaces.map((i) => i.name), contains('eth0'));
    });
  });

  group('GrpcClient.version', () {
    test('returns the version string', () async {
      _stub.onVersion = (_) => Stream.value(VersionReply()..version = '1.15.0');
      expect(await _client.version(), equals('1.15.0'));
    });

    test('calls checkForUpdate when reply carries update info', () async {
      final info = UpdateInfo()..version = '4.0.0';
      _stub.onVersion = (_) => Stream.value(VersionReply()..updateInfo = info);
      await _client.version();
      expect(providerContainer.read(updateProvider).version, equals('4.0.0'));
    });
  });

  group('GrpcClient.info', () {
    test('returns details from the InfoReply', () async {
      _stub.onInfo = (_) => Stream.value(
            InfoReply()..details.add(DetailedInfoItem()..name = 'my-vm'),
          );
      final details = await _client.info();
      expect(details.map((d) => d.name), contains('my-vm'));
    });

    test('filters by name when names are provided', () async {
      InfoRequest? captured;
      _stub.onInfo = (req) {
        captured = req;
        return Stream.value(InfoReply());
      };
      await _client.info(['vm-a']);
      expect(
        captured?.instanceSnapshotPairs.map((p) => p.instanceName).toList(),
        equals(['vm-a']),
      );
    });
  });

  group('GrpcClient.get', () {
    test('sends the key and returns the value', () async {
      String? capturedKey;
      _stub.onGet = (req) {
        capturedKey = req.key;
        return Stream.value(GetReply()..value = 'some-value');
      };
      final result = await _client.get('my.key');
      expect(capturedKey, equals('my.key'));
      expect(result, equals('some-value'));
    });
  });

  group('GrpcClient.set', () {
    test('sends both key and value', () async {
      SetRequest? captured;
      _stub.onSet = (req) {
        captured = req;
        return Stream.value(SetReply());
      };
      await _client.set('my.key', 'new-value');
      expect(captured?.key, equals('my.key'));
      expect(captured?.val, equals('new-value'));
    });
  });

  group('GrpcClient.sshInfo', () {
    test('returns SSHInfo for the requested vm', () async {
      _stub.onSshInfo = (_) => Stream.value(
            SSHInfoReply()..sshInfo['my-vm'] = (SSHInfo()..host = '10.0.0.1'),
          );
      final info = await _client.sshInfo('my-vm');
      expect(info?.host, equals('10.0.0.1'));
    });
  });

  group('GrpcClient.daemonInfo', () {
    test('returns the DaemonInfoReply from the server', () async {
      _stub.onDaemonInfo = (_) => Stream.value(DaemonInfoReply()..cpus = 4);
      final reply = await _client.daemonInfo();
      expect(reply.cpus, equals(4));
    });
  });
}
