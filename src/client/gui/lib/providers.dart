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
import 'logger.dart';

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
    channelShutdownHandler: () => logger.w('gRPC channel shut down'),
  )));
});

final vmInfosStreamProvider = StreamProvider<List<VmInfo>>((ref) async* {
  final grpcClient = ref.watch(grpcClientProvider);
  // this is to de-duplicate errors received from the stream
  Object? lastError;
  while (true) {
    final timer = Future.delayed(1900.milliseconds);
    try {
      final infos = await grpcClient.info();
      yield infos
          .where((info) => info.instanceStatus.status != Status.DELETED)
          .toList();
      lastError = null;
    } catch (error, stackTrace) {
      if (error != lastError) {
        logger.e('Error on polling info', error: error, stackTrace: stackTrace);
        yield* Stream.error(error, stackTrace);
      }
      lastError = error;
    }
    // these two timers make it so that requests are sent with at least a 2s pause between them
    // but if the request takes longer than 1.9s to complete, we still wait 100ms before sending the next one
    await timer;
    await Future.delayed(100.milliseconds);
  }
});

final daemonAvailableProvider = Provider((ref) {
  final error = ref.watch(vmInfosStreamProvider).error;
  if (error == null) return true;
  if (error case GrpcError grpcError) {
    final message = grpcError.message ?? '';
    if (message.contains('failed to obtain exit status for remote process')) {
      return true;
    }
    if (message.contains('Connection is being forcefully terminated')) {
      return true;
    }
  }
  return false;
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
  final file = File(settingsFile());

  @override
  String build(String arg) {
    file.parent.create(recursive: true).then((dir) => dir
        .watch()
        .where((event) => event.path == file.path)
        .first
        .whenComplete(() => Timer(250.milliseconds, ref.invalidateSelf)));
    return getSetting(arg);
  }

  void set(String value) => setSetting(arg, value);

  @override
  bool updateShouldNotify(String previous, String next) => previous != next;
}

const primaryNameKey = 'client.primary-name';
final clientSettingProvider = NotifierProvider.autoDispose
    .family<ClientSettingNotifier, String, String>(ClientSettingNotifier.new);

class DaemonSettingNotifier
    extends AutoDisposeFamilyAsyncNotifier<String, String> {
  @override
  Future<String> build(String arg) async {
    return ref.watch(daemonAvailableProvider)
        ? await ref.watch(grpcClientProvider).get(arg)
        : state.valueOrNull ?? await Completer<String>().future;
  }

  Future<void> set(String value) async {
    state = AsyncValue.data(value);
    try {
      await ref.read(grpcClientProvider).set(arg, value);
    } catch (_) {
      Timer(100.milliseconds, ref.invalidateSelf);
      rethrow;
    }
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
final daemonSettingProvider = AsyncNotifierProvider.autoDispose
    .family<DaemonSettingNotifier, String, String>(DaemonSettingNotifier.new);

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

final networksProvider = Provider.autoDispose((ref) {
  final driver = ref.watch(daemonSettingProvider(driverKey)).valueOrNull;
  if (driver != null && ref.watch(daemonAvailableProvider)) {
    ref.watch(grpcClientProvider).networks().then((networks) {
      ref.state = networks.map((n) => n.name).toBuiltSet();
    }).ignore();
  }
  return BuiltSet<String>();
});
