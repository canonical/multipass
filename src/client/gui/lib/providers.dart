import 'dart:async';
import 'dart:io';

import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:collection/collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';
import 'package:grpc/grpc.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'ffi.dart';
import 'grpc_client.dart';
import 'logger.dart';

export 'grpc_client.dart';

late final ProviderContainer providerContainer;

final sharedPreferencesProvider = Provider<SharedPreferences>((ref) {
  throw UnimplementedError('SharedPreferences must be overridden');
});

final ffiAvailableProvider = Provider((ref) {
  return isFFIAvailable;
});

final grpcClientProvider = Provider((ref) {
  // Check if FFI is available first
  if (!ref.watch(ffiAvailableProvider)) {
    throw ffiLoadError ?? Exception('FFI library not available');
  }

  final address = getServerAddress();
  final certPair = getCertPair();
  final rootCert = getRootCert();

  var channelCredentials = CustomChannelCredentials(
    authority: 'localhost',
    certificate: certPair.cert,
    certificateKey: certPair.key,
    rootCertificate: rootCert,
  );

  return GrpcClient(
    RpcClient(
      ClientChannel(
        address.scheme == InternetAddressType.unix.name.toLowerCase()
            ? InternetAddress(address.path, type: InternetAddressType.unix)
            : address.host,
        port: address.port,
        options: ChannelOptions(credentials: channelCredentials),
        channelShutdownHandler: () => logger.w('gRPC channel shut down'),
      ),
    ),
  );
});

final pollingProvider = StreamProvider<({List<VmInfo> info, List<Zone> zones})>(
  (ref) async* {
    final grpcClient = ref.watch(grpcClientProvider);
    // this is to de-duplicate errors received from the stream
    Object? lastError;
    while (true) {
      final timer = Future.delayed(1900.milliseconds);
      try {
        final [info, zones] = await Future.wait(
          [grpcClient.info(), grpcClient.zones()],
          eagerError: true,
        );
        yield (info: info as List<VmInfo>, zones: zones as List<Zone>);
        lastError = null;
      } catch (error, stackTrace) {
        if (error != lastError) {
          logger.e('Error on polling info',
              error: error, stackTrace: stackTrace);
          yield* Stream.error(error, stackTrace);
        }
        lastError = error;
      }
      // these two timers make it so that requests are sent with at least a 2s pause between them
      // but if the request takes longer than 1.9s to complete, we still wait 100ms before sending the next one
      await timer;
      await Future.delayed(100.milliseconds);
    }
  },
);

final daemonAvailableProvider = Provider((ref) {
  final error = ref.watch(pollingProvider).error;
  if (error == null) return true;
  if (error case GrpcError grpcError) {
    final message = grpcError.message ?? '';
    if (message.contains('failed to obtain exit status for remote process')) {
      return true;
    }
  }
  return false;
});

final daemonInfoProvider = FutureProvider((ref) {
  return ref.watch(grpcClientProvider).daemonInfo();
});

class AllVmInfosNotifier extends Notifier<List<DetailedInfoItem>> {
  @override
  List<DetailedInfoItem> build() {
    return ref.watch(pollingProvider).when(
          data: (data) => data.info,
          loading: () => const [],
          error: (_, __) => const [],
        );
  }

  Future<void> update() async {
    state = await ref.read(grpcClientProvider).info();
  }
}

final allVmInfosProvider =
    NotifierProvider<AllVmInfosNotifier, List<DetailedInfoItem>>(
  AllVmInfosNotifier.new,
);

final vmInfosProvider = Provider((ref) {
  final existingVms = ref
      .watch(allVmInfosProvider)
      .where((info) => info.instanceStatus.status != Status.DELETED)
      .toBuiltList();
  final existingVmNames = existingVms.map((i) => i.name).toSet();
  final launchingVms = ref.watch(launchingVmsProvider).where((info) {
    return !existingVmNames.contains(info.name);
  });

  return existingVms.concat(launchingVms).sortedBy((i) => i.name).toList();
});

final vmInfosMapProvider = Provider((ref) {
  return {for (final i in ref.watch(vmInfosProvider)) i.name: i};
});

class VmInfoNotifier extends Notifier<DetailedInfoItem> {
  VmInfoNotifier(this.arg);
  final String arg;

  @override
  DetailedInfoItem build() {
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

final deletedVmsProvider = Provider((ref) {
  return ref
      .watch(allVmInfosProvider)
      .where((info) => info.instanceStatus.status == Status.DELETED)
      .map((info) => info.name)
      .toBuiltSet();
});

final zonesProvider = Provider<BuiltList<Zone>>((ref) {
  return ref.watch(pollingProvider).when(
        data: (data) => data.zones.build(),
        loading: () => BuiltList(),
        error: (_, __) => BuiltList(),
      );
});

class LaunchingVmsNotifier extends Notifier<BuiltList<DetailedInfoItem>> {
  @override
  BuiltList<DetailedInfoItem> build() {
    final vms = stateOrNull ?? BuiltList();

    return vms;
  }

  void add(LaunchRequest request) {
    final vms = state;
    state = vms.rebuild((builder) {
      builder.add(DetailedInfoItem(
        name: request.instanceName,
        cpuCount: request.numCores.toString(),
        diskTotal: request.diskSpace,
        memoryTotal: request.memSize,
        zone: Zone(name: request.zone),
        instanceInfo: InstanceDetails(
          currentRelease: request.image,
        ),
      ));
    });
  }

  void remove(String name) {
    final vms = state;
    state = vms.rebuild((builder) {
      builder.removeWhere((info) => info.name == name);
    });
  }

  @override
  bool updateShouldNotify(
    BuiltList<DetailedInfoItem> previous,
    BuiltList<DetailedInfoItem> next,
  ) {
    return previous != next;
  }
}

final launchingVmsProvider =
    NotifierProvider<LaunchingVmsNotifier, BuiltList<DetailedInfoItem>>(
  LaunchingVmsNotifier.new,
);

final isLaunchingProvider = Provider.autoDispose.family<bool, String>((
  ref,
  name,
) {
  final launchingVms = ref.watch(launchingVmsProvider);
  return launchingVms.any((info) => info.name == name);
});

class ClientSettingNotifier extends Notifier<String> {
  ClientSettingNotifier(this.arg);
  final String arg;
  final file = File(settingsFile());

  @override
  String build() {
    file.parent.create(recursive: true).then(
          (dir) => dir
              .watch()
              .where((event) => event.path == file.path)
              .first
              .whenComplete(() => Timer(250.milliseconds, ref.invalidateSelf)),
        );
    return getSetting(arg);
  }

  void set(String value) => setSetting(arg, value);

  @override
  bool updateShouldNotify(String previous, String next) => previous != next;
}

const primaryNameKey = 'client.primary-name';
final clientSettingProvider = NotifierProvider.autoDispose
    .family<ClientSettingNotifier, String, String>(ClientSettingNotifier.new);

class DaemonSettingNotifier extends AsyncNotifier<String> {
  DaemonSettingNotifier(this.arg);
  final String arg;

  @override
  Future<String> build() async {
    return ref.watch(daemonAvailableProvider)
        ? await ref.watch(grpcClientProvider).get(arg)
        : state.when(
            data: (data) => data,
            loading: () => throw StateError('Daemon not available'),
            error: (_, __) => throw StateError('Daemon not available'),
          );
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

final trayMenuDataProvider = Provider.autoDispose((ref) {
  return ref.watch(daemonAvailableProvider)
      ? ref.watch(vmStatusesProvider)
      : null;
});

final daemonVersionProvider = NotifierProvider<DaemonVersionNotifier, String>(
  DaemonVersionNotifier.new,
);

class DaemonVersionNotifier extends Notifier<String> {
  @override
  String build() {
    if (ref.watch(daemonAvailableProvider)) {
      ref
          .watch(grpcClientProvider)
          .version()
          .catchError((_) => 'failed to get version')
          .then((version) => state = version);
    }
    return 'loading...';
  }
}

const driverKey = 'local.driver';
const bridgedNetworkKey = 'local.bridged-network';
const privilegedMountsKey = 'local.privileged-mounts';
const passphraseKey = 'local.passphrase';
final daemonSettingProvider = AsyncNotifierProvider.autoDispose
    .family<DaemonSettingNotifier, String, String>(DaemonSettingNotifier.new);

enum VmResource { cpus, memory, disk, bridged }

typedef VmResourceKey = ({String name, VmResource resource});

class VmResourceNotifier extends AsyncNotifier<String> {
  VmResourceNotifier(this.arg);
  final VmResourceKey arg;

  @override
  Future<String> build() async {
    final (:name, :resource) = arg;
    final launchingVm = ref.watch(
      launchingVmsProvider.select((infos) {
        return infos.firstWhereOrNull((info) => info.name == name);
      }),
    );

    if (launchingVm != null) {
      return switch (resource) {
        VmResource.cpus => launchingVm.cpuCount,
        VmResource.memory => launchingVm.memoryTotal,
        VmResource.disk => launchingVm.diskTotal,
        VmResource.bridged => 'false',
      };
    }

    final key = 'local.$name.${resource.name}';
    return await ref.watch(daemonSettingProvider(key).future);
  }

  Future<void> set(String value) async {
    final (:name, :resource) = arg;
    final key = 'local.$name.${resource.name}';
    ref.read(daemonSettingProvider(key).notifier).set(value);
  }
}

final vmResourceProvider = AsyncNotifierProvider.autoDispose
    .family<VmResourceNotifier, String, VmResourceKey>(VmResourceNotifier.new);

class GuiSettingNotifier extends Notifier<String?> {
  GuiSettingNotifier(this.arg);
  final String arg;

  @override
  String? build() {
    final sharedPreferences = ref.read(sharedPreferencesProvider);
    return sharedPreferences.getString(arg);
  }

  void set(String value) {
    final sharedPreferences = ref.read(sharedPreferencesProvider);
    sharedPreferences.setString(arg, value);
    state = value;
  }

  @override
  bool updateShouldNotify(String? previous, String? next) => previous != next;
}

const onAppCloseKey = 'onAppClose';
const hotkeyKey = 'hotkey';
const askTerminalCloseKey = 'askTerminalClose';
final guiSettingProvider = NotifierProvider.autoDispose
    .family<GuiSettingNotifier, String?, String>(GuiSettingNotifier.new);

final networksProvider =
    FutureProvider.autoDispose<BuiltSet<String>>((ref) async {
  final driver = ref.watch(daemonSettingProvider(driverKey)).when(
        data: (data) => data,
        loading: () => null,
        error: (_, __) => null,
      );
  if (driver != null && ref.watch(daemonAvailableProvider)) {
    final networks = await ref.watch(grpcClientProvider).networks();
    return BuiltSet<String>(networks);
  }
  return BuiltSet<String>();
});

// Session-level terminal font size that resets on app restart
class SessionTerminalFontSizeNotifier extends Notifier<double> {
  static const defaultFontSize = 13.0;

  @override
  double build() => defaultFontSize;

  void set(double value) => state = value;
}

final sessionTerminalFontSizeProvider =
    NotifierProvider<SessionTerminalFontSizeNotifier, double>(
        SessionTerminalFontSizeNotifier.new);
