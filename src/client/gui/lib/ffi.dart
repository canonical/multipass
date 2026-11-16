import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:ffi/ffi.dart';

import 'platform/platform.dart';
import 'ffi_ssh_coordinates.dart';

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

class FFILibrary {
  final ffi.DynamicLibrary? _lib;
  final Exception? _loadError;

  FFILibrary._({ffi.DynamicLibrary? lib, Exception? loadError})
      : _lib = lib,
        _loadError = loadError;

  factory FFILibrary._load() {
    try {
      final lib = ffi.DynamicLibrary.open(mpPlatform.ffiLibraryName);
      return FFILibrary._(lib: lib);
    } catch (e) {
      return FFILibrary._(
        loadError: Exception('Failed to load libdart_ffi library: $e'),
      );
    }
  }

  ffi.DynamicLibrary get lib {
    if (_lib == null) {
      throw _loadError!;
    }
    return _lib!;
  }

  bool get isAvailable => _lib != null;
  Exception? get loadError => _loadError;
}

final _ffiLib = FFILibrary._load();
final _lib = _ffiLib.lib;

bool get isFFIAvailable => _ffiLib.isAvailable;
Exception? get ffiLoadError => _ffiLib.loadError;

final _multipassVersion = _lib
    .lookupFunction<ffi.Pointer<Utf8> Function(), ffi.Pointer<Utf8> Function()>(
  'multipass_version',
);

final _generatePetname = _lib
    .lookupFunction<ffi.Pointer<Utf8> Function(), ffi.Pointer<Utf8> Function()>(
  'generate_petname',
);

final _getServerAddress = _lib
    .lookupFunction<ffi.Pointer<Utf8> Function(), ffi.Pointer<Utf8> Function()>(
  'get_server_address',
);

enum _SettingsResult { ok, keyNotFound, invalidValue, unexpectedError }

extension on int {
  _SettingsResult get settingsResult => _SettingsResult.values[this];
}

final _settingsFile = _lib
    .lookupFunction<ffi.Pointer<Utf8> Function(), ffi.Pointer<Utf8> Function()>(
  'settings_file',
);

final _getSetting = _lib.lookupFunction<
    ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Pointer<ffi.Pointer<Utf8>>),
    int Function(
        ffi.Pointer<Utf8>, ffi.Pointer<ffi.Pointer<Utf8>>)>('get_setting');

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
final defaultId = _lib.lookupFunction<ffi.Int32 Function(), int Function()>(
  'default_id',
);

final _memoryInBytes = _lib.lookupFunction<
    ffi.LongLong Function(ffi.Pointer<Utf8>),
    int Function(ffi.Pointer<Utf8>)>('memory_in_bytes');

final _humanReadableMemory = _lib.lookupFunction<
    ffi.Pointer<Utf8> Function(ffi.LongLong),
    ffi.Pointer<Utf8> Function(int)>('human_readable_memory');

final getTotalDiskSize =
    _lib.lookupFunction<ffi.LongLong Function(), int Function()>(
  'get_total_disk_size',
);

final _defaultMountTarget = _lib.lookupFunction<
    ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>),
    ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8>)>('default_mount_target');

final _openVsockSocket = _lib.lookupFunction<
    ffi.IntPtr Function(ffi.Pointer<FfiSSHCoordinates>),
    int Function(ffi.Pointer<FfiSSHCoordinates>)>('open_vsock_socket');

final _shutdownSocket =
    _lib.lookupFunction<ffi.Void Function(ffi.IntPtr), void Function(int)>(
        'shutdown_socket');

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

final _getRootCert = _lib
    .lookupFunction<ffi.Pointer<Utf8> Function(), ffi.Pointer<Utf8> Function()>(
  'get_root_cert',
);

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

List<int> getRootCert() {
  return utf8.encode(_getRootCert().string);
}

/// Connects to the guest over the vsock-family transport in [coordinates] and
/// returns a connected, blocking socket descriptor (an int fd on POSIX, a
/// SOCKET handle on Windows). Returns a negative value on failure.
int openVsockSocket(ffi.Pointer<FfiSSHCoordinates> coordinates) {
  return _openVsockSocket(coordinates);
}

/// Shuts down both directions of a descriptor from [openVsockSocket] so a
/// blocked `read()`/`recv()` sees EOF.
void shutdownSocket(int socket) {
  _shutdownSocket(socket);
}

// --- Blocking socket I/O bindings for a descriptor from [openVsockSocket] ---
// dart:io can't adopt a pre-existing descriptor, so the read/write isolates in
// RawFdSSHSocket call these platform primitives directly. The native side
// returns an int fd on POSIX and a SOCKET handle on Windows; a SOCKET is
// pointer-sized (UINT_PTR), so it is marshalled as an [ffi.IntPtr] to avoid
// truncation on 64-bit Windows. A single Dart signature is exposed per
// operation; the Winsock recv/send entry points take an extra flags argument
// that is wrapped away.
typedef ReadWriteFn = int Function(
    int socket, ffi.Pointer<ffi.Uint8> buf, int count);
typedef CloseFn = int Function(int socket);

// POSIX: ssize_t read/write(int fd, void* buf, size_t count); int close(int fd).
typedef _PosixReadWriteNative = ffi.Long Function(
    ffi.Int32 fd, ffi.Pointer<ffi.Uint8> buf, ffi.Size count);
typedef _PosixCloseNative = ffi.Int32 Function(ffi.Int32 fd);

// Winsock (ws2_32.dll): int recv/send(SOCKET s, char* buf, int len, int flags);
// int closesocket(SOCKET s). SOCKET is pointer-sized, len/return are 32-bit.
typedef _WinsockRecvSendNative = ffi.Int32 Function(ffi.IntPtr socket,
    ffi.Pointer<ffi.Uint8> buf, ffi.Int32 len, ffi.Int32 flags);
typedef _WinsockRecvSendDart = int Function(
    int socket, ffi.Pointer<ffi.Uint8> buf, int len, int flags);
typedef _WinsockCloseNative = ffi.Int32 Function(ffi.IntPtr socket);

// On Windows the descriptor is a raw SOCKET, so I/O goes through Winsock; on
// POSIX it is a real fd handled by libc.
final ffi.DynamicLibrary _socketIoLib = Platform.isWindows
    ? ffi.DynamicLibrary.open('ws2_32.dll')
    : ffi.DynamicLibrary.process();

// Adapts a Winsock recv/send to the flag-less [_ReadWriteFn] signature.
ReadWriteFn _wrapWinsockRecvSend(String symbol) {
  final fn = _socketIoLib
      .lookupFunction<_WinsockRecvSendNative, _WinsockRecvSendDart>(symbol);
  return (int socket, ffi.Pointer<ffi.Uint8> buf, int count) =>
      fn(socket, buf, count, 0);
}

/// Blocking read from a socket descriptor returned by [openVsockSocket]
/// (`recv` on Windows, `read` on POSIX).
final ReadWriteFn nativeSocketRead = Platform.isWindows
    ? _wrapWinsockRecvSend('recv')
    : _socketIoLib.lookupFunction<_PosixReadWriteNative, ReadWriteFn>('read');

/// Blocking write to a socket descriptor returned by [openVsockSocket]
/// (`send` on Windows, `write` on POSIX).
final ReadWriteFn nativeSocketWrite = Platform.isWindows
    ? _wrapWinsockRecvSend('send')
    : _socketIoLib.lookupFunction<_PosixReadWriteNative, ReadWriteFn>('write');

/// Closes a socket descriptor returned by [openVsockSocket]
/// (`closesocket` on Windows, `close` on POSIX).
final CloseFn nativeSocketClose = Platform.isWindows
    ? _socketIoLib.lookupFunction<_WinsockCloseNative, CloseFn>('closesocket')
    : _socketIoLib.lookupFunction<_PosixCloseNative, CloseFn>('close');

// Windows surfaces the last socket error via WSAGetLastError (thread-local like
// errno). Null on non-Windows targets.
final int Function()? _wsaGetLastError = Platform.isWindows
    ? _socketIoLib
        .lookupFunction<ffi.Int32 Function(), int Function()>('WSAGetLastError')
    : null;

// Resolves the C `errno` address on POSIX; the symbol differs per platform, null
// if not found or on Windows.
final ffi.Pointer<ffi.Int32> Function()? _getErrnoPtr = Platform.isWindows
    ? null
    : () {
        try {
          if (Platform.isMacOS || Platform.isIOS) {
            return _socketIoLib.lookupFunction<
                ffi.Pointer<ffi.Int32> Function(),
                ffi.Pointer<ffi.Int32> Function()>('__error');
          } else {
            return _socketIoLib.lookupFunction<
                ffi.Pointer<ffi.Int32> Function(),
                ffi.Pointer<ffi.Int32> Function()>('__errno_location');
          }
        } catch (_) {
          return null;
        }
      }();

/// Whether the last socket syscall failed with EINTR (POSIX, 4) / WSAEINTR
/// (Windows, 10004). Both are thread-local, so query on the same isolate that
/// made the syscall.
bool isSocketEintr() {
  if (Platform.isWindows) {
    return _wsaGetLastError != null && _wsaGetLastError!() == 10004;
  }
  return _getErrnoPtr != null && _getErrnoPtr!().value == 4;
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
  switch (_setSetting(
    key.toNativeUtf8(),
    value.toNativeUtf8(),
    output,
  ).settingsResult) {
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
