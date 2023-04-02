import 'dart:io';

import 'package:async/async.dart';
import 'package:basics/int_basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';

import 'generated/multipass.pbgrpc.dart';
import 'globals.dart';

final grpcClientProvider = Provider(
  (_) {
    final address = getDaemonAddress();
    final certDir = '${getDataDirectory()}/multipass-client-certificate';

    var channelCredentials = CustomChannelCredentials(
      authority: 'localhost',
      certificate: File('$certDir/multipass_cert.pem').readAsBytesSync(),
      certificateKey: File('$certDir/multipass_cert_key.pem').readAsBytesSync(),
    );

    return GrpcClient(RpcClient(ClientChannel(
      address,
      port: Platform.isWindows ? 50051 : 0,
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

  Future<StartReply?> start(Iterable<String> names) async {
    var request = StartRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.start(Stream.value(request)).firstOrNull;
  }

  Future<StopReply?> stop(Iterable<String> names) async {
    var request = StopRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.stop(Stream.value(request)).firstOrNull;
  }

  Future<SuspendReply?> suspend(Iterable<String> names) async {
    var request = SuspendRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.suspend(Stream.value(request)).firstOrNull;
  }

  Future<RestartReply?> restart(Iterable<String> names) async {
    var request = RestartRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.restart(Stream.value(request)).firstOrNull;
  }

  Future<DeleteReply?> delete(Iterable<String> names) async {
    var request = DeleteRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.delet(Stream.value(request)).firstOrNull;
  }

  Future<RecoverReply?> recover(Iterable<String> names) async {
    var request = RecoverRequest(
      instanceNames: InstanceNames(instanceName: names),
    );
    return _client.recover(Stream.value(request)).firstOrNull;
  }

  Future<DeleteReply?> purge(Iterable<String> names) async {
    var request = DeleteRequest(
      instanceNames: InstanceNames(instanceName: names),
      purge: true,
    );
    return _client.delet(Stream.value(request)).firstOrNull;
  }

  Stream<List<VmInfo>> infoStream() async* {
    Object? lastError;
    await for (final _ in Stream.periodic(1.seconds)) {
      try {
        final reply = await _client.info(Stream.value(InfoRequest())).last;
        yield reply.info;
        lastError = null;
      } catch (error, stackTrace) {
        if (error != lastError) yield* Stream.error(error, stackTrace);
        lastError = error;
      }
    }
  }

  Future<List<FindReply_ImageInfo>> find() {
    return _client
        .find(Stream.value(FindRequest(showBlueprints: true, showImages: true)))
        .single
        .then((r) => r.imagesInfo);
  }

  Future<String> version() {
    return _client
        .version(Stream.value(VersionRequest()))
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

Object getDaemonAddress() {
  final customAddress = Platform.environment['MULTIPASS_SERVER_ADDRESS'];
  if (Platform.isLinux) {
    return InternetAddress(
      customAddress ??
          '${Platform.environment["SNAP_COMMON"] ?? "/run"}/multipass_socket',
      type: InternetAddressType.unix,
    );
  } else if (Platform.isWindows) {
    return customAddress ?? 'localhost';
  } else if (Platform.isMacOS) {
    return InternetAddress(
      customAddress ?? "/var/run/multipass_socket",
      type: InternetAddressType.unix,
    );
  } else {
    throw const OSError("Platform not supported");
  }
}

String getDataDirectory() {
  final customDir = Platform.environment['MULTIPASS_DATA_DIRECTORY'];
  if (customDir != null) return customDir;

  if (Platform.isLinux) {
    return Platform.environment['SNAP_NAME'] == 'multipass'
        ? '${Platform.environment['SNAP_USER_DATA']!}/data'
        : '${Platform.environment['HOME']!}/.local/share';
  } else if (Platform.isWindows) {
    return Platform.environment['LocalAppData']!;
  } else if (Platform.isMacOS) {
    return '${Platform.environment["HOME"]!}/Library/Application Support';
  } else {
    throw const OSError("Platform not supported");
  }
}
