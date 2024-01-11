import 'dart:io';

import 'package:async/async.dart';
import 'package:fpdart/fpdart.dart';
import 'package:grpc/grpc.dart';
import 'package:rxdart/rxdart.dart';

import 'generated/multipass.pbgrpc.dart';

export 'generated/multipass.pbgrpc.dart';

typedef Status = InstanceStatus_Status;
typedef VmInfo = DetailedInfoItem;
typedef ImageInfo = FindReply_ImageInfo;

class GrpcClient {
  final RpcClient _client;

  GrpcClient(this._client);

  Stream<Either<LaunchReply, MountReply>> launch(
    LaunchRequest request, [
    List<MountRequest> mountRequests = const [],
  ]) {
    Stream<Either<LaunchReply, MountReply>> launchImpl() async* {
      yield* _client.launch(Stream.value(request)).map(Either.left);
      for (final mountRequest in mountRequests) {
        yield* _client.mount(Stream.value(mountRequest)).map(Either.right);
      }
    }

    return BehaviorSubject()..addStream(launchImpl(), cancelOnError: true);
  }

  Future<StartReply?> start(Iterable<String> names) {
    final request = StartRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.start(Stream.value(request)).firstOrNull;
  }

  Future<StopReply?> stop(Iterable<String> names) {
    final request = StopRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.stop(Stream.value(request)).firstOrNull;
  }

  Future<SuspendReply?> suspend(Iterable<String> names) {
    final request = SuspendRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.suspend(Stream.value(request)).firstOrNull;
  }

  Future<RestartReply?> restart(Iterable<String> names) {
    final request = RestartRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.restart(Stream.value(request)).firstOrNull;
  }

  Future<DeleteReply?> delete(Iterable<String> names) {
    final request = DeleteRequest(
      instanceSnapshotPairs: names.map(
        (name) => InstanceSnapshotPair(instanceName: name),
      ),
    );
    return _client.delet(Stream.value(request)).firstOrNull;
  }

  Future<RecoverReply?> recover(Iterable<String> names) {
    final request = RecoverRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.recover(Stream.value(request)).firstOrNull;
  }

  Future<DeleteReply?> purge(Iterable<String> names) {
    final request = DeleteRequest(
      instanceSnapshotPairs: names.map(
        (name) => InstanceSnapshotPair(instanceName: name),
      ),
      purge: true,
    );
    return _client.delet(Stream.value(request)).firstOrNull;
  }

  Future<List<VmInfo>> info([Iterable<String> names = const []]) {
    final request = InfoRequest(
      instanceSnapshotPairs: names.map(
        (name) => InstanceSnapshotPair(instanceName: name),
      ),
    );
    return _client
        .info(Stream.value(request))
        .single
        .then((r) => r.details.toList());
  }

  Future<FindReply> find({bool images = true, bool blueprints = true}) {
    final request = FindRequest(
      showImages: images,
      showBlueprints: blueprints,
    );
    return _client.find(Stream.value(request)).single;
  }

  Future<String> version() {
    final request = VersionRequest();
    return _client
        .version(Stream.value(request))
        .single
        .then((reply) => reply.version);
  }

  Future<UpdateInfo> updateInfo() {
    final request = VersionRequest();
    return _client
        .version(Stream.value(request))
        .single
        .then((reply) => reply.updateInfo);
  }

  Future<String> get(String key) {
    final request = GetRequest(key: key);
    return _client
        .get(Stream.value(request))
        .single
        .then((reply) => reply.value);
  }

  Future<void> set(String key, String value) {
    final request = SetRequest(key: key, val: value);
    return _client.set(Stream.value(request)).single;
  }
}

class CustomChannelCredentials extends ChannelCredentials {
  final List<int> certificateChain;
  final List<int> certificateKey;

  CustomChannelCredentials({
    required List<int> certificate,
    required this.certificateKey,
    String? authority,
  })  : certificateChain = certificate,
        super.secure(
            certificates: certificate,
            authority: authority,
            onBadCertificate: allowBadCertificates);

  @override
  SecurityContext get securityContext {
    final ctx = super.securityContext!;
    ctx.useCertificateChainBytes(certificateChain);
    ctx.usePrivateKeyBytes(certificateKey);
    return ctx;
  }
}
