import 'dart:async';
import 'dart:io';

import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';
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
  final sub = Stream.periodic(1.seconds).listen((_) => ref.notifyListeners());
  ref.onDispose(sub.cancel);
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

final vmInfosMapProvider = Provider((ref) {
  return {for (final info in ref.watch(vmInfosProvider)) info.name: info};
});

class VmInfoNotifier
    extends AutoDisposeFamilyNotifier<DetailedInfoItem, String> {
  @override
  DetailedInfoItem build(String arg) {
    return ref.watch(vmInfosMapProvider)[arg] ?? DetailedInfoItem();
  }
}

final vmInfoProvider = NotifierProvider.autoDispose
    .family<VmInfoNotifier, DetailedInfoItem, String>(VmInfoNotifier.new);

final vmStatusesProvider = Provider((ref) {
  return ref
      .watch(vmInfosMapProvider)
      .mapValue((info) => info.instanceStatus.status)
      .build();
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

class DaemonSettingNotifier2
    extends AutoDisposeFamilyAsyncNotifier<String, String> {
  @override
  Future<String> build(String arg) async {
    return ref.watch(daemonAvailableProvider)
        ? await ref.watch(grpcClientProvider).get(arg)
        : state.valueOrNull ?? await Completer<String>().future;
  }

  void set(String value) async {
    state = await AsyncValue.guard(() async {
      await ref.read(grpcClientProvider).set(arg, value);
      return value;
    });
  }

  @override
  bool updateShouldNotify(
    AsyncValue<String> previous,
    AsyncValue<String> next,
  ) {
    return previous != next;
  }
}

const driverKey = 'local.driver';
const bridgedNetworkKey = 'local.bridged-network';
const privilegedMountsKey = 'local.privileged-mounts';
const passphraseKey = 'local.passphrase';
final daemonSettingProvider = NotifierProvider.autoDispose
    .family<DaemonSettingNotifier, String?, String>(DaemonSettingNotifier.new);

final daemonSettingProvider2 = AsyncNotifierProvider.autoDispose
    .family<DaemonSettingNotifier2, String, String>(DaemonSettingNotifier2.new);

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
