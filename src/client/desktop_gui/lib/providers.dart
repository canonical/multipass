import 'dart:io';

import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';

import 'ffi.dart';
import 'grpc_client.dart';

export 'grpc_client.dart';

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

final vmInfosStreamProvider = StreamProvider<List<VmInfo>>(
  (ref) async* {
    // this is to de-duplicate errors received from the stream
    final grpcClient = ref.watch(grpcClientProvider);
    Object? lastError;
    await for (final _ in Stream.periodic(1.seconds)) {
      try {
        yield await grpcClient.info();
        lastError = null;
      } catch (error, stackTrace) {
        if (error != lastError) yield* Stream.error(error, stackTrace);
        lastError = error;
      }
    }
  },
);

final daemonAvailableProvider = Provider(
  (ref) => !ref.watch(vmInfosStreamProvider).hasError,
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

const primaryNameKey = 'client.primary-name';
const _clientSettingsKeys = [primaryNameKey];

final clientSettingsProvider = Provider<BuiltMap<String, String>>((ref) {
  Stream.periodic(1.seconds).listen(
    (_) => ref.state = BuiltMap.of({
      for (final key in _clientSettingsKeys) key: getSetting(key),
    }),
  );
  return {for (final key in _clientSettingsKeys) key: ''}.build();
});
