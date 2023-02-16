import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart' as grpc;

import 'generated/multipass.pbgrpc.dart';

class ChannelCredentials extends grpc.ChannelCredentials {
  final List<int> certificateChain;
  final List<int> certificateKey;

  ChannelCredentials({
    required List<int> certificate,
    required this.certificateKey,
    String? authority,
  })  : certificateChain = certificate,
        super.secure(
            certificates: certificate,
            authority: authority,
            onBadCertificate: grpc.allowBadCertificates);

  @override
  SecurityContext get securityContext {
    final ctx = super.securityContext!;
    ctx.useCertificateChainBytes(certificateChain);
    ctx.usePrivateKeyBytes(certificateKey);
    return ctx;
  }
}

getDaemonAddress() {
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

getDataDirectory() {
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

final grpcClient = Provider(
  (_) {
    final address = getDaemonAddress();
    print(address);
    final certDir = '${getDataDirectory()}/multipass-client-certificate';
    print(certDir);

    return RpcClient(grpc.ClientChannel(
      address,
      port: Platform.isWindows ? 50051 : 0,
      options: grpc.ChannelOptions(
        credentials: ChannelCredentials(
          authority: 'localhost',
          certificate: File('$certDir/multipass_cert.pem').readAsBytesSync(),
          certificateKey:
              File('$certDir/multipass_cert_key.pem').readAsBytesSync(),
        ),
      ),
    ));
  },
);

final vmsInfo = StreamProvider((ref) async* {
  final client = ref.watch(grpcClient);
  await for (final _ in Stream.periodic(const Duration(seconds: 1))) {
    print('${DateTime.now()} Polling daemon');
    yield await client.info(Stream.value(InfoRequest())).single;
  }
});
