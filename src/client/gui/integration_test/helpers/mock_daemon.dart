import 'dart:collection';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/providers.dart';

enum _MockMode { strict, nice }

/// In-process gRPC server that stands in for the real multipassd daemon.
///
/// Tests control responses using two mechanisms:
///
/// - **One-shot queues** (`enqueue*`): push handlers consumed in order.  Each
///   call pops the front handler; the queue must be empty at teardown.
/// - **Repeating fallbacks** (`on*`): a function called for every request that
///   has no queued handler.  Useful for steady-state background polling.
///
/// Choose a mode at construction:
/// - `MockDaemon.strict()`: fails the test on any unexpected RPC call.
/// - `MockDaemon.nice()`: returns empty success replies for unconfigured
///   calls; good for tests that only care about one specific RPC.
///
/// Call [serve] before pumping the app, [shutdown] in `addTearDown`, and
/// [assertAllConsumed] at the end of any test that used `enqueue*`.
class MockDaemon extends RpcServiceBase {
  MockDaemon.strict() : _mode = _MockMode.strict;
  MockDaemon.nice() : _mode = _MockMode.nice;

  final _MockMode _mode;

  late Server _server;
  late ClientChannel _channel;

  GrpcClient get client => GrpcClient(RpcClient(_channel));

  // One-shot queues: consumed in FIFO order; each enqueued handler fires
  // exactly once.  Use enqueue* in tests that need precise per-call control.
  final _infoQ = Queue<Stream<InfoReply> Function(InfoRequest)>();
  final _startQ = Queue<Stream<StartReply> Function(StartRequest)>();
  final _stopQ = Queue<Stream<StopReply> Function(StopRequest)>();
  final _suspendQ = Queue<Stream<SuspendReply> Function(SuspendRequest)>();
  final _restartQ = Queue<Stream<RestartReply> Function(RestartRequest)>();
  final _deletQ = Queue<Stream<DeleteReply> Function(DeleteRequest)>();
  final _recoverQ = Queue<Stream<RecoverReply> Function(RecoverRequest)>();
  final _purgeQ = Queue<Stream<PurgeReply> Function(PurgeRequest)>();
  final _launchQ = Queue<Stream<LaunchReply> Function(LaunchRequest)>();
  final _findQ = Queue<Stream<FindReply> Function(FindRequest)>();
  final _networksQ = Queue<Stream<NetworksReply> Function(NetworksRequest)>();
  final _versionQ = Queue<Stream<VersionReply> Function(VersionRequest)>();
  final _getQ = Queue<Stream<GetReply> Function(GetRequest)>();
  final _setQ = Queue<Stream<SetReply> Function(SetRequest)>();
  final _sshInfoQ = Queue<Stream<SSHInfoReply> Function(SSHInfoRequest)>();
  final _daemonInfoQ =
      Queue<Stream<DaemonInfoReply> Function(DaemonInfoRequest)>();

  // Repeating fallbacks: called when no queued handler exists for a method.
  Stream<InfoReply> Function(InfoRequest)? onInfo;
  Stream<StartReply> Function(StartRequest)? onStart;
  Stream<StopReply> Function(StopRequest)? onStop;
  Stream<SuspendReply> Function(SuspendRequest)? onSuspend;
  Stream<RestartReply> Function(RestartRequest)? onRestart;
  Stream<DeleteReply> Function(DeleteRequest)? onDelete;
  Stream<RecoverReply> Function(RecoverRequest)? onRecover;
  Stream<PurgeReply> Function(PurgeRequest)? onPurge;
  Stream<LaunchReply> Function(LaunchRequest)? onLaunch;
  Stream<FindReply> Function(FindRequest)? onFind;
  Stream<NetworksReply> Function(NetworksRequest)? onNetworks;
  Stream<VersionReply> Function(VersionRequest)? onVersion;
  Stream<GetReply> Function(GetRequest)? onGet;
  Stream<SetReply> Function(SetRequest)? onSet;
  Stream<SSHInfoReply> Function(SSHInfoRequest)? onSshInfo;
  Stream<DaemonInfoReply> Function(DaemonInfoRequest)? onDaemonInfo;

  // Enqueue helpers: add a handler to the back of the one-shot queue.
  void enqueueInfo(Stream<InfoReply> Function(InfoRequest) h) => _infoQ.add(h);
  void enqueueStart(Stream<StartReply> Function(StartRequest) h) =>
      _startQ.add(h);
  void enqueueStop(Stream<StopReply> Function(StopRequest) h) => _stopQ.add(h);
  void enqueueSuspend(Stream<SuspendReply> Function(SuspendRequest) h) =>
      _suspendQ.add(h);
  void enqueueRestart(Stream<RestartReply> Function(RestartRequest) h) =>
      _restartQ.add(h);
  void enqueueDelete(Stream<DeleteReply> Function(DeleteRequest) h) =>
      _deletQ.add(h);
  void enqueueRecover(Stream<RecoverReply> Function(RecoverRequest) h) =>
      _recoverQ.add(h);
  void enqueuePurge(Stream<PurgeReply> Function(PurgeRequest) h) =>
      _purgeQ.add(h);
  void enqueueLaunch(Stream<LaunchReply> Function(LaunchRequest) h) =>
      _launchQ.add(h);
  void enqueueFind(Stream<FindReply> Function(FindRequest) h) => _findQ.add(h);
  void enqueueNetworks(Stream<NetworksReply> Function(NetworksRequest) h) =>
      _networksQ.add(h);
  void enqueueVersion(Stream<VersionReply> Function(VersionRequest) h) =>
      _versionQ.add(h);
  void enqueueGet(Stream<GetReply> Function(GetRequest) h) => _getQ.add(h);
  void enqueueSet(Stream<SetReply> Function(SetRequest) h) => _setQ.add(h);
  void enqueueSshInfo(Stream<SSHInfoReply> Function(SSHInfoRequest) h) =>
      _sshInfoQ.add(h);
  void enqueueDaemonInfo(
          Stream<DaemonInfoReply> Function(DaemonInfoRequest) h) =>
      _daemonInfoQ.add(h);

  // Dispatch: queue first, then fallback, then mode-dependent default.
  Stream<R> _handle<Q, R>(
    Queue<Stream<R> Function(Q)> queue,
    Stream<R> Function(Q)? fallback,
    R Function() emptyDefault,
    String methodName,
    Q req,
  ) {
    if (queue.isNotEmpty) return queue.removeFirst()(req);
    if (fallback != null) return fallback(req);
    if (_mode == _MockMode.strict) {
      fail('MockDaemon: unexpected call to $methodName\nRequest: $req');
    }
    return Stream.value(emptyDefault());
  }

  static Stream<R> _unimplemented<R>(String name) =>
      Stream.error(GrpcError.unimplemented('MockDaemon: $name not supported'));

  // Server lifecycle: bind to loopback on an ephemeral port so tests run
  // in parallel without port conflicts.
  Future<void> serve() async {
    _server = Server.create(services: [this]);
    await _server.serve(address: InternetAddress.loopbackIPv4, port: 0);
    _channel = ClientChannel(
      'localhost',
      port: _server.port!,
      options: const ChannelOptions(credentials: ChannelCredentials.insecure()),
    );
  }

  Future<void> shutdown() async {
    await _channel.shutdown();
    await _server.shutdown();
  }

  /// Asserts every one-shot queue is empty.
  ///
  /// Call at the end of any test that used `enqueue*` to catch handlers that
  /// were never triggered which signals the expected RPC was never sent.
  void assertAllConsumed() {
    final namedQueues = [
      ('info', _infoQ),
      ('start', _startQ),
      ('stop', _stopQ),
      ('suspend', _suspendQ),
      ('restart', _restartQ),
      ('delete', _deletQ),
      ('recover', _recoverQ),
      ('purge', _purgeQ),
      ('launch', _launchQ),
      ('find', _findQ),
      ('networks', _networksQ),
      ('version', _versionQ),
      ('get', _getQ),
      ('set', _setQ),
      ('sshInfo', _sshInfoQ),
      ('daemonInfo', _daemonInfoQ),
    ];
    for (final (name, queue) in namedQueues) {
      expect(
        queue,
        isEmpty,
        reason:
            'MockDaemon: $name queue has ${queue.length} unconsumed handler(s)',
      );
    }
  }

  // RpcServiceBase overrides
  @override
  Stream<InfoReply> info(ServiceCall c, Stream<InfoRequest> req) async* {
    yield* _handle(_infoQ, onInfo, InfoReply.new, 'info', await req.first);
  }

  @override
  Stream<StartReply> start(ServiceCall c, Stream<StartRequest> req) async* {
    yield* _handle(_startQ, onStart, StartReply.new, 'start', await req.first);
  }

  @override
  Stream<StopReply> stop(ServiceCall c, Stream<StopRequest> req) async* {
    yield* _handle(_stopQ, onStop, StopReply.new, 'stop', await req.first);
  }

  @override
  Stream<SuspendReply> suspend(
      ServiceCall c, Stream<SuspendRequest> req) async* {
    yield* _handle(
        _suspendQ, onSuspend, SuspendReply.new, 'suspend', await req.first);
  }

  @override
  Stream<RestartReply> restart(
      ServiceCall c, Stream<RestartRequest> req) async* {
    yield* _handle(
        _restartQ, onRestart, RestartReply.new, 'restart', await req.first);
  }

  @override
  Stream<DeleteReply> delet(ServiceCall c, Stream<DeleteRequest> req) async* {
    yield* _handle(
        _deletQ, onDelete, DeleteReply.new, 'delete', await req.first);
  }

  @override
  Stream<RecoverReply> recover(
      ServiceCall c, Stream<RecoverRequest> req) async* {
    yield* _handle(
        _recoverQ, onRecover, RecoverReply.new, 'recover', await req.first);
  }

  @override
  Stream<PurgeReply> purge(ServiceCall c, Stream<PurgeRequest> req) async* {
    yield* _handle(_purgeQ, onPurge, PurgeReply.new, 'purge', await req.first);
  }

  @override
  Stream<LaunchReply> launch(ServiceCall c, Stream<LaunchRequest> req) async* {
    yield* _handle(
        _launchQ, onLaunch, LaunchReply.new, 'launch', await req.first);
  }

  @override
  Stream<FindReply> find(ServiceCall c, Stream<FindRequest> req) async* {
    yield* _handle(_findQ, onFind, FindReply.new, 'find', await req.first);
  }

  @override
  Stream<NetworksReply> networks(
      ServiceCall c, Stream<NetworksRequest> req) async* {
    yield* _handle(
        _networksQ, onNetworks, NetworksReply.new, 'networks', await req.first);
  }

  @override
  Stream<VersionReply> version(
      ServiceCall c, Stream<VersionRequest> req) async* {
    yield* _handle(
        _versionQ, onVersion, VersionReply.new, 'version', await req.first);
  }

  @override
  Stream<GetReply> get(ServiceCall c, Stream<GetRequest> req) async* {
    yield* _handle(_getQ, onGet, GetReply.new, 'get', await req.first);
  }

  @override
  Stream<SetReply> set(ServiceCall c, Stream<SetRequest> req) async* {
    yield* _handle(_setQ, onSet, SetReply.new, 'set', await req.first);
  }

  @override
  Stream<SSHInfoReply> ssh_info(
      ServiceCall c, Stream<SSHInfoRequest> req) async* {
    yield* _handle(
        _sshInfoQ, onSshInfo, SSHInfoReply.new, 'ssh_info', await req.first);
  }

  @override
  Stream<DaemonInfoReply> daemon_info(
      ServiceCall c, Stream<DaemonInfoRequest> req) async* {
    yield* _handle(_daemonInfoQ, onDaemonInfo, DaemonInfoReply.new,
        'daemon_info', await req.first);
  }

  // RPCs the GUI never calls. Always return UNIMPLEMENTED so a test fails
  // immediately if the app unexpectedly invokes one of these.
  @override
  Stream<LaunchReply> create(ServiceCall c, Stream<LaunchRequest> r) =>
      _unimplemented('create');
  @override
  Stream<ListReply> list(ServiceCall c, Stream<ListRequest> r) =>
      _unimplemented('list');
  @override
  Stream<MountReply> mount(ServiceCall c, Stream<MountRequest> r) =>
      _unimplemented('mount');
  @override
  Stream<UmountReply> umount(ServiceCall c, Stream<UmountRequest> r) =>
      _unimplemented('umount');

  @override
  Future<PingReply> ping(ServiceCall c, PingRequest r) =>
      Future.error(GrpcError.unimplemented('MockDaemon: ping not supported'));
  @override
  Stream<KeysReply> keys(ServiceCall c, Stream<KeysRequest> r) =>
      _unimplemented('keys');
  @override
  Stream<AuthenticateReply> authenticate(
          ServiceCall c, Stream<AuthenticateRequest> r) =>
      _unimplemented('authenticate');
  @override
  Stream<SnapshotReply> snapshot(ServiceCall c, Stream<SnapshotRequest> r) =>
      _unimplemented('snapshot');
  @override
  Stream<RestoreReply> restore(ServiceCall c, Stream<RestoreRequest> r) =>
      _unimplemented('restore');
  @override
  Stream<CloneReply> clone(ServiceCall c, Stream<CloneRequest> r) =>
      _unimplemented('clone');
  @override
  Stream<WaitReadyReply> wait_ready(
          ServiceCall c, Stream<WaitReadyRequest> r) =>
      _unimplemented('wait_ready');
  @override
  Stream<ZonesReply> zones(ServiceCall c, Stream<ZonesRequest> r) =>
      _unimplemented('zones');
  @override
  Stream<ZonesStateReply> zones_state(
          ServiceCall c, Stream<ZonesStateRequest> r) =>
      _unimplemented('zones_state');
}
