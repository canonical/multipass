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

  Future<StartReply?> start(Iterable<String> names) {
    final request = StartRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    logger.i('Sent ${request.repr}');
    return _client
        .start(Stream.value(request))
        .doOnData(checkForUpdate)
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<StopReply?> stop(Iterable<String> names) {
    final request = StopRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    logger.i('Sent ${request.repr}');
    return _client
        .stop(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<SuspendReply?> suspend(Iterable<String> names) {
    final request = SuspendRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    logger.i('Sent ${request.repr}');
    return _client
        .suspend(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<RestartReply?> restart(Iterable<String> names) {
    final request = RestartRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    logger.i('Sent ${request.repr}');
    return _client
        .restart(Stream.value(request))
        .doOnData(checkForUpdate)
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<DeleteReply?> delete(Iterable<String> names) {
    final request = DeleteRequest(
      instanceSnapshotPairs: names.map(
        (name) => InstanceSnapshotPair(instanceName: name),
      ),
    );
    logger.i('Sent ${request.repr}');
    return _client
        .delet(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<RecoverReply?> recover(Iterable<String> names) {
    final request = RecoverRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    logger.i('Sent ${request.repr}');
    return _client
        .recover(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<DeleteReply?> purge(Iterable<String> names) {
    final request = DeleteRequest(
      instanceSnapshotPairs: names.map(
        (name) => InstanceSnapshotPair(instanceName: name),
      ),
      purge: true,
    );
    logger.i('Sent ${request.repr}');
    return _client
        .delet(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<List<VmInfo>> info([Iterable<String> names = const []]) {
    final request = InfoRequest(
      instanceSnapshotPairs: names.map(
        (name) => InstanceSnapshotPair(instanceName: name),
      ),
    );
    return _client
        .info(Stream.value(request))
        .doOnData(checkForUpdate)
        .last
        .then((r) => r.details.toList());
  }

  Future<MountReply?> mount(MountRequest request) {
    logger.i('Sent ${request.repr}');
    return _client
        .mount(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<void> umount(String name, [String? path]) {
    final request = UmountRequest(
      targetPaths: [TargetPathInfo(instanceName: name, targetPath: path)],
    );
    logger.i('Sent ${request.repr}');
    return _client
        .umount(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<FindReply> find({bool images = true, bool blueprints = true}) {
    final request = FindRequest(
      showImages: images,
      showBlueprints: blueprints,
    );
    logger.i('Sent ${request.repr}');
    return _client.find(Stream.value(request)).doOnEach(logGrpc(request)).last;
  }

  Future<List<NetInterface>> networks() {
    final request = NetworksRequest();
    logger.i('Sent ${request.repr}');
    return _client
        .networks(Stream.value(request))
        .doOnData(checkForUpdate)
        .doOnEach(logGrpc(request))
        .last
        .then((r) => r.interfaces);
  }

  Future<String> version() {
    final request = VersionRequest();
    logger.i('Sent ${request.repr}');
    return _client
        .version(Stream.value(request))
        .doOnData(checkForUpdate)
        .doOnEach(logGrpc(request))
        .last
        .then((reply) => reply.version);
  }

  Future<String> get(String key) {
    final request = GetRequest(key: key);
    logger.i('Sent ${request.repr}');
    return _client
        .get(Stream.value(request))
        .doOnEach(logGrpc(request))
        .last
        .then((reply) => reply.value);
  }

  Future<void> set(String key, String value) {
    final request = SetRequest(key: key, val: value);
    logger.i('Sent ${request.repr}');
    return _client
        .set(Stream.value(request))
        .doOnEach(logGrpc(request))
        .lastOrNull;
  }

  Future<SSHInfo?> sshInfo(String name) {
    final request = SSHInfoRequest(instanceName: [name]);
    logger.i('Sent ${request.repr}');
    return _client
        .ssh_info(Stream.value(request))
        .doOnEach(logGrpc(request))
        .last
        .then((reply) => reply.sshInfo[name]);
  }

  Future<DaemonInfoReply> daemonInfo() {
    final request = DaemonInfoRequest();
    logger.i('Sent ${request.repr}');
    return _client
        .daemon_info(Stream.value(request))
        .doOnEach(logGrpc(request))
        .last;
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
