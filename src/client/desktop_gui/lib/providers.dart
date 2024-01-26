import 'dart:async';
import 'dart:io';

import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'ffi.dart';
import 'grpc_client.dart';

export 'grpc_client.dart';

final grpcClientProvider = Provider((_) {
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
});

final pollTickProvider = Provider<void>((ref) {
  Stream.periodic(1.seconds).listen((_) => ref.notifyListeners());
});

final vmInfosStreamProvider = StreamProvider<List<VmInfo>>((ref) {
  final grpcClient = ref.watch(grpcClientProvider);
  final controller = StreamController<List<VmInfo>>();
  // this is to de-duplicate errors received from the stream
  Object? lastError;
  ref.listen(pollTickProvider, (_, __) async {
    try {
      controller.add(await grpcClient.info());
      lastError = null;
    } catch (error, stackTrace) {
      if (error != lastError) controller.addError(error, stackTrace);
      lastError = error;
    }
  }, fireImmediately: true);

  return controller.stream;
});

final daemonAvailableProvider = Provider((ref) {
  return !ref.watch(vmInfosStreamProvider).hasError;
});

final vmInfosProvider = Provider((ref) {
  return ref.watch(vmInfosStreamProvider).valueOrNull ?? const [];
});

Map<String, Status> infosToStatusMap(Iterable<VmInfo> infos) {
  return {for (final info in infos) info.name: info.instanceStatus.status};
}

final vmStatusesProvider = Provider((ref) {
  return infosToStatusMap(ref.watch(vmInfosProvider)).build();
});

final vmNamesProvider = Provider((ref) {
  return ref.watch(vmStatusesProvider).keys.toBuiltSet();
});

class ClientSettingNotifier extends AutoDisposeFamilyNotifier<String, String> {
  @override
  String build(String arg) {
    ref.watch(pollTickProvider);
    return getSetting(arg);
  }

  void set(String value) {
    setSetting(arg, value);
    state = value;
  }

  @override
  bool updateShouldNotify(String previous, String next) => previous != next;
}

const primaryNameKey = 'client.primary-name';
final clientSettingProvider = NotifierProvider.autoDispose
    .family<ClientSettingNotifier, String, String>(ClientSettingNotifier.new);

class DaemonSettingNotifier extends AutoDisposeFamilyNotifier<String?, String> {
  @override
  String? build(String arg) {
    if (ref.watch(daemonAvailableProvider)) {
      final grpcClient = ref.watch(grpcClientProvider);
      grpcClient.get(arg).then((value) => state = value).ignore();
    }

    return stateOrNull;
  }

  void set(String value) {
    ref.read(grpcClientProvider).set(arg, value).ignore();
    state = value;
  }

  @override
  bool updateShouldNotify(String? previous, String? next) => previous != next;
}

const driverKey = 'local.driver';
const bridgedNetworkKey = 'local.bridged-network';
const privilegedMountsKey = 'local.privileged-mounts';
const passphraseKey = 'local.passphrase';
final daemonSettingProvider = NotifierProvider.autoDispose
    .family<DaemonSettingNotifier, String?, String>(DaemonSettingNotifier.new);

class GuiSettingNotifier extends AutoDisposeFamilyNotifier<String?, String> {
  final SharedPreferences sharedPreferences;

  GuiSettingNotifier(this.sharedPreferences);

  @override
  String? build(String arg) {
    return sharedPreferences.getString(arg);
  }

  void set(String value) {
    sharedPreferences.setString(arg, value);
    state = value;
  }

  @override
  bool updateShouldNotify(String? previous, String? next) => previous != next;
}

const onAppCloseKey = 'onAppClose';
const hotkeyKey = 'hotkey';
// this provider is set with a value obtained asynchronously in main.dart
final guiSettingProvider = NotifierProvider.autoDispose
    .family<GuiSettingNotifier, String?, String>(() {
  throw UnimplementedError();
});
