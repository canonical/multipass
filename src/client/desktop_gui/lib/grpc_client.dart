import 'dart:io';

import 'package:async/async.dart';
import 'package:basics/int_basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';

import 'ffi.dart';
import 'generated/multipass.pbgrpc.dart';

typedef Status = InstanceStatus_Status;
typedef VmInfo = DetailedInfoItem;

final grpcClientProvider = Provider(
  (_) {
    final address = getServerAddress();
    final certPair = getCertPair();

    var channelCredentials = CustomChannelCredentials(
      authority: 'localhost',
      certificate: certPair.cert,
      certificateKey: certPair.key,
    );

    return GrpcClient(RpcClient(ClientChannel(
      address.scheme == InternetAddressType.unix.name.toLowerCase()
          ? InternetAddress(address.path, type: InternetAddressType.unix)
          : address.host,
      port: address.port,
      options: ChannelOptions(credentials: channelCredentials),
    )));
  },
);

final vmInfosStreamProvider = StreamProvider(
  (ref) => ref.watch(grpcClientProvider).infoStream(),
);

final vmInfosProvider = Provider(
  (ref) => ref.watch(vmInfosStreamProvider).valueOrNull ?? const [],
);

Map<String, Status> infosToStatusMap(Iterable<VmInfo> infos) {
  return {for (final info in infos) info.name: info.instanceStatus.status};
}

final vmStatusesProvider = Provider(
  (ref) => infosToStatusMap(ref.watch(vmInfosProvider)).build(),
);

final vmNamesProvider = Provider(
  (ref) => ref.watch(vmStatusesProvider).keys.toBuiltSet(),
);

class GrpcClient {
  final RpcClient _client;

  GrpcClient(this._client);

  ResponseStream<LaunchReply> launch(LaunchRequest request) {
    return _client.launch(Stream.value(request), options: CallOptions());
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

  Stream<List<VmInfo>> infoStream() async* {
    // this is to de-duplicate errors received from the stream
    Object? lastError;
    await for (final _ in Stream.periodic(1.seconds)) {
      try {
        final reply = await _client.info(Stream.value(InfoRequest())).last;
        yield reply.details;
        lastError = null;
      } catch (error, stackTrace) {
        if (error != lastError) yield* Stream.error(error, stackTrace);
        lastError = error;
      }
    }
  }

  Future<List<FindReply_ImageInfo>> find() {
    final request = FindRequest(showImages: true, showBlueprints: true);
    return _client.find(Stream.value(request)).single.then((r) => r.imagesInfo);
  }

  Future<String> version() {
    final request = VersionRequest();
    return _client
        .version(Stream.value(request))
        .single
        .then((reply) => reply.version);
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
