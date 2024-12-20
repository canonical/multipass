import 'dart:async';
import 'dart:io';

import 'package:fpdart/fpdart.dart';
import 'package:grpc/grpc.dart';
import 'package:protobuf/protobuf.dart' hide RpcClient;
import 'package:rxdart/rxdart.dart';

import 'logger.dart';
import 'providers.dart';
import 'update_available.dart';

export 'generated/multipass.pbgrpc.dart';

typedef Status = InstanceStatus_Status;
typedef VmInfo = DetailedInfoItem;
typedef ImageInfo = FindReply_ImageInfo;
typedef MountPaths = MountInfo_MountPaths;
typedef RpcMessage = GeneratedMessage;

extension on RpcMessage {
  String get repr => '$runtimeType${toProto3Json()}';
}

void checkForUpdate(RpcMessage message) {
  final updateInfo = switch (message) {
    LaunchReply launchReply => launchReply.updateInfo,
    InfoReply infoReply => infoReply.updateInfo,
    ListReply listReply => listReply.updateInfo,
    NetworksReply networksReply => networksReply.updateInfo,
    StartReply startReply => startReply.updateInfo,
    RestartReply restartReply => restartReply.updateInfo,
    VersionReply versionReply => versionReply.updateInfo,
    _ => UpdateInfo(),
  };

  providerContainer.read(updateProvider.notifier).set(updateInfo);
}

void Function(StreamNotification<RpcMessage>) logGrpc(RpcMessage request) {
  return (notification) {
    switch (notification.kind) {
      case NotificationKind.data:
        final reply = notification.requireDataValue.deepCopy();
        if (reply is SSHInfoReply) {
          for (final info in reply.sshInfo.values) {
            info.privKeyBase64 = '*hidden*';
          }
        }
        if (reply is LaunchReply) {
          final percent = reply.launchProgress.percentComplete;
          if (!['0', '100', '-1'].contains(percent)) return;
        }
        logger.i('${request.repr} received ${reply.repr}');
      case NotificationKind.error:
        final es = notification.errorAndStackTraceOrNull;
        logger.e(
          '${request.repr} received an error',
          error: es?.error,
          stackTrace: es?.stackTrace,
        );
      case NotificationKind.done:
        logger.i('${request.repr} is done');
    }
  };
}

class GrpcClient {
  final RpcClient _client;

  GrpcClient(this._client);

  Stream<Either<LaunchReply, MountReply>?> launch(
    LaunchRequest request, {
    List<MountRequest> mountRequests = const [],
    Future<void>? cancel,
  }) async* {
    logger.i('Sent ${request.repr}');
    final launchStream = _client.launch(Stream.value(request));
    cancel?.then((_) => launchStream.cancel());
    yield* launchStream
        .doOnData(checkForUpdate)
        .doOnEach(logGrpc(request))
        .map(Either.left);
    for (final mountRequest in mountRequests) {
      logger.i('Sent ${mountRequest.repr}');
      yield* _client
          .mount(Stream.value(mountRequest))
          .doOnEach(logGrpc(mountRequest))
          .map(Either.right);
    }
  }

  Future<Rep?> doRpc<Req extends RpcMessage, Rep extends RpcMessage>(
    ResponseStream<Rep> Function(Stream<Req> request) action,
    Req request, {
    bool checkUpdates = false,
    bool log = true,
  }) {
    if (log) logger.i('Sent ${request.repr}');
    Stream<Rep> replyStream = action(Stream.value(request));
    if (checkUpdates) replyStream = replyStream.doOnData(checkForUpdate);
    if (log) replyStream = replyStream.doOnEach(logGrpc(request));
    return replyStream.lastOrNull;
  }

  Future<StartReply?> start(Iterable<String> names) {
    return doRpc(
      _client.start,
      StartRequest(instanceNames: InstanceNames(instanceName: names)),
      checkUpdates: true,
    );
  }

  Future<StopReply?> stop(Iterable<String> names) {
    return doRpc(
      _client.stop,
      StopRequest(instanceNames: InstanceNames(instanceName: names)),
    );
  }

  Future<SuspendReply?> suspend(Iterable<String> names) {
    return doRpc(
      _client.suspend,
      SuspendRequest(instanceNames: InstanceNames(instanceName: names)),
    );
  }

  Future<RestartReply?> restart(Iterable<String> names) {
    return doRpc(
      _client.restart,
      RestartRequest(instanceNames: InstanceNames(instanceName: names)),
      checkUpdates: true,
    );
  }

  Future<DeleteReply?> delete(Iterable<String> names) {
    return doRpc(
      _client.delet,
      DeleteRequest(
        instanceSnapshotPairs: names.map(
          (name) => InstanceSnapshotPair(instanceName: name),
        ),
      ),
    );
  }

  Future<RecoverReply?> recover(Iterable<String> names) {
    return doRpc(
      _client.recover,
      RecoverRequest(instanceNames: InstanceNames(instanceName: names)),
    );
  }

  Future<DeleteReply?> purge(Iterable<String> names) {
    return doRpc(
      _client.delet,
      DeleteRequest(
        purge: true,
        instanceSnapshotPairs: names.map(
          (name) => InstanceSnapshotPair(instanceName: name),
        ),
      ),
    );
  }

  Future<List<VmInfo>> info([Iterable<String> names = const []]) {
    return doRpc(
      _client.info,
      checkUpdates: true,
      log: false,
      InfoRequest(
        instanceSnapshotPairs: names.map(
          (name) => InstanceSnapshotPair(instanceName: name),
        ),
      ),
    ).then((r) => r!.details.toList());
  }

  Future<MountReply?> mount(MountRequest request) {
    return doRpc(_client.mount, request);
  }

  Future<void> umount(String name, [String? path]) {
    return doRpc(
      _client.umount,
      UmountRequest(
        targetPaths: [TargetPathInfo(instanceName: name, targetPath: path)],
      ),
    );
  }

  Future<FindReply> find({bool images = true, bool blueprints = true}) {
    return doRpc(
      _client.find,
      FindRequest(
        showImages: images,
        showBlueprints: blueprints,
      ),
    ).then((r) => r!);
  }

  Future<List<NetInterface>> networks() {
    return doRpc(
      _client.networks,
      NetworksRequest(),
      checkUpdates: true,
    ).then((r) => r!.interfaces);
  }

  Future<String> version() {
    return doRpc(
      _client.version,
      VersionRequest(),
      checkUpdates: true,
    ).then((r) => r!.version);
  }

  Future<String> get(String key) {
    return doRpc(_client.get, GetRequest(key: key)).then((r) => r!.value);
  }

  Future<void> set(String key, String value) {
    return doRpc(_client.set, SetRequest(key: key, val: value));
  }

  Future<SSHInfo?> sshInfo(String name) {
    return doRpc(
      _client.ssh_info,
      SSHInfoRequest(instanceName: [name]),
    ).then((r) => r!.sshInfo[name]);
  }

  Future<DaemonInfoReply> daemonInfo() {
    return doRpc(_client.daemon_info, DaemonInfoRequest()).then((r) => r!);
  }
}

class CustomChannelCredentials extends ChannelCredentials {
  final List<int> certificateChain;
  final List<int> certificateKey;

  CustomChannelCredentials({
    super.authority,
    required List<int> certificate,
    required this.certificateKey,
  })  : certificateChain = certificate,
        super.secure(
          certificates: certificate,
          onBadCertificate: allowBadCertificates,
        );

  @override
  SecurityContext get securityContext {
    final ctx = super.securityContext!;
    ctx.useCertificateChainBytes(certificateChain);
    ctx.usePrivateKeyBytes(certificateKey);
    return ctx;
  }
}

extension<T> on Stream<T> {
  Future<T?> get lastOrNull {
    final completer = Completer<T?>.sync();
    T? result;
    listen(
      (event) => result = event,
      onError: completer.completeError,
      onDone: () => completer.complete(result),
      cancelOnError: true,
    );
    return completer.future;
  }
}
