import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:ffi/ffi.dart';

String _libraryName(String baseName) {
  if (Platform.isLinux) return 'lib$baseName.so';
  if (Platform.isWindows) return '$baseName.dll';
  if (Platform.isMacOS) return 'lib$baseName.dylib';
  throw const OSError('OS not supported');
}

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

final _lib = ffi.DynamicLibrary.open(_libraryName('dart_ffi'));

final _multipassVersion = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('multipass_version');

final _generatePetname = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('generate_petname');

final _getServerAddress = _lib.lookupFunction<ffi.Pointer<Utf8> Function(),
    ffi.Pointer<Utf8> Function()>('get_server_address');

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

String generatePetname() {
  return _generatePetname().string;
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
