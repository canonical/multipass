import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:ffi/ffi.dart';

import 'platform/platform.dart';

extension on ffi.Pointer<Utf8> {
  String get string {
    if (this == ffi.nullptr) {
      throw Exception("Couldn't retrieve data through FFI");
    }
    final string = toDartString();
    malloc.free(this);
    return string;
  }
}

final _lib = ffi.DynamicLibrary.open(mpPlatform.ffiLibraryName);

final _multipassVersion = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('multipass_version');

final _generatePetname = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('generate_petname');

final _getServerAddress = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('get_server_address');

enum _SettingsResult {
  ok,
  keyNotFound,
  invalidValue,
  unexpectedError,
}

extension on int {
  _SettingsResult get settingsResult => _SettingsResult.values[this];
}

final _settingsFile = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('settings_file');

final _getSetting = _lib.lookupFunction<
    ffi.Int32 Function(
      ffi.Pointer<Utf8>,
      ffi.Pointer<ffi.Pointer<Utf8>>,
    ),
    int Function(
      ffi.Pointer<Utf8>,
      ffi.Pointer<ffi.Pointer<Utf8>>,
    )>('get_setting');

final _setSetting = _lib.lookupFunction<
    ffi.Int32 Function(
      ffi.Pointer<Utf8>,
      ffi.Pointer<Utf8>,
      ffi.Pointer<ffi.Pointer<Utf8>>,
    ),
    int Function(
      ffi.Pointer<Utf8>,
      ffi.Pointer<Utf8>,
      ffi.Pointer<ffi.Pointer<Utf8>>,
    )>('set_setting');

final uid = _lib.lookupFunction<ffi.Int32 Function(), int Function()>('uid');
final gid = _lib.lookupFunction<ffi.Int32 Function(), int Function()>('gid');
final defaultId =
    _lib.lookupFunction<ffi.Int32 Function(), int Function()>('default_id');

final _memoryInBytes = _lib.lookupFunction<
    ffi.LongLong Function(ffi.Pointer<Utf8>),
    int Function(ffi.Pointer<Utf8>)>('memory_in_bytes');

final _humanReadableMemory = _lib.lookupFunction<
    ffi.Pointer<Utf8> Function(ffi.LongLong),
    ffi.Pointer<Utf8> Function(int)>('human_readable_memory');

final getTotalDiskSize =
    _lib.lookupFunction<ffi.LongLong Function(), int Function()>(
        'get_total_disk_size');

final _defaultMountTarget = _lib.lookupFunction<
    ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>),
    ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>)>('default_mount_target');

final class _NativeKeyCertificatePair extends ffi.Struct {
  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> pem_cert;

  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> pem_cert_key;
}

class KeyCertificatePair {
  final List<int> cert;
  final List<int> key;

  KeyCertificatePair(this.cert, this.key);
}

final _getCertPair = _lib.lookupFunction<_NativeKeyCertificatePair Function(),
    _NativeKeyCertificatePair Function()>('get_cert_pair');

String get multipassVersion => _multipassVersion().toDartString();

String generatePetname([Iterable<String> existing = const []]) {
  while (true) {
    final name = _generatePetname().string;
    if (!existing.contains(name)) return name;
  }
}

Uri getServerAddress() {
  final address = _getServerAddress().string;
  final unixRegex = RegExp('unix:(.+)');
  final unixSocketPath = unixRegex.firstMatch(address)?.group(1);
  if (unixSocketPath != null) {
    return Uri(scheme: InternetAddressType.unix.name, path: unixSocketPath);
  }
  final tcpRegex = RegExp(r'^(.+):(\d+)$');
  final tcpMatch = tcpRegex.firstMatch(address);
  if (tcpMatch != null) {
    return Uri(host: tcpMatch.group(1), port: int.parse(tcpMatch.group(2)!));
  }

  throw Exception("Couldn't retrieve data through FFI");
}

KeyCertificatePair getCertPair() {
  final pair = _getCertPair();
  final cert = utf8.encode(pair.pem_cert.string);
  final key = utf8.encode(pair.pem_cert_key.string);
  return KeyCertificatePair(cert, key);
}

String settingsFile() => _settingsFile().string;

String getSetting(String key) {
  final output = malloc<ffi.Pointer<Utf8>>();
  switch (_getSetting(key.toNativeUtf8(), output).settingsResult) {
    case _SettingsResult.ok:
      final result = output.value.string;
      malloc.free(output);
      return result;
    case _SettingsResult.keyNotFound:
      malloc.free(output);
      throw ArgumentError.value(key, 'key', 'client settings key not found');
    case _SettingsResult.unexpectedError:
      final result = output.value.string;
      malloc.free(output);
      throw Exception("failed retrieving client setting '$key': $result");
    default:
      malloc.free(output);
      throw UnimplementedError();
  }
}

void setSetting(String key, String value) {
  final output = malloc<ffi.Pointer<Utf8>>();
  switch (_setSetting(key.toNativeUtf8(), value.toNativeUtf8(), output)
      .settingsResult) {
    case _SettingsResult.ok:
      malloc.free(output);
    case _SettingsResult.keyNotFound:
      malloc.free(output);
      throw ArgumentError.value(key, 'key', 'client settings key not found');
    case _SettingsResult.invalidValue:
      final result = output.value.string;
      malloc.free(output);
      throw ArgumentError.value(value, 'value', result);
    case _SettingsResult.unexpectedError:
      final result = output.value.string;
      malloc.free(output);
      throw Exception("failed storing client setting '$key'='$value': $result");
  }
}

String humanReadableMemory(int bytes) => _humanReadableMemory(bytes).string;

int? memoryInBytes(String value) {
  final result = _memoryInBytes(value.toNativeUtf8());
  return result == -1 ? null : result;
}

String defaultMountTarget({required String source}) {
  return _defaultMountTarget(source.toNativeUtf8()).string;
}
