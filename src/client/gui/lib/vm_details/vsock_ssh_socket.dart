import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:dartssh2/dartssh2.dart';
import 'package:ffi/ffi.dart';

import '../ffi.dart';
import '../ssh_coordinates_ffi.dart';

/// An [SSHSocket] that tries a native vsock-family transport
/// (HVSOCK/VSOCK/USOCK) first and falls back to plain TCP if it is unavailable.
///
/// [openVsockSocket] connects natively and returns a descriptor that
/// [RawFdSSHSocket] then drives as a regular socket. The transport tag only
/// tells the native side how to connect — the Dart path is identical.
///
/// Inert until [connect]; afterwards all members delegate to whichever
/// connection won. Owns [_coordinates] and frees it on [close]/[destroy].
class DualSSHSocket implements SSHSocket {
  DualSSHSocket(this._coordinates);

  final Pointer<FfiSSHCoordinates> _coordinates;

  SSHSocket? _delegate;

  SSHSocket get _socket =>
      _delegate ??
      (throw StateError('DualSSHSocket used before connect() completed'));

  /// Connects, preferring the vsock-family transport and falling back to TCP.
  Future<void> connect({Duration? timeout}) async {
    try {
      _delegate = await _connectVsock(timeout: timeout);
    } catch (_) {
      _delegate = await SSHSocket.connect(
        _coordinates.ref.tcp_host.toDartString(),
        _coordinates.ref.port,
        timeout: timeout,
      );
    }
  }

  /// Asks the native side to connect; throws on an unset transport or an
  /// invalid descriptor so [connect] falls back to TCP.
  Future<SSHSocket> _connectVsock({Duration? timeout}) async {
    if (_coordinates.ref.vsock_tag == vsockNone) {
      throw StateError('No vsock host set; falling back to TCP');
    }

    final fd = openVsockSocket(_coordinates);
    if (fd < 0) {
      throw StateError('vsock transport unavailable; falling back to TCP');
    }

    return RawFdSSHSocket(fd);
  }

  @override
  Stream<Uint8List> get stream => _socket.stream;

  @override
  StreamSink<List<int>> get sink => _socket.sink;

  @override
  Future<void> get done => _socket.done;

  @override
  Future<void> close() {
    freeFfiSSHCoordinates(_coordinates);
    return _socket.close();
  }

  @override
  void destroy() {
    freeFfiSSHCoordinates(_coordinates);
    _socket.destroy();
  }
}

class _FlushCommand {
  final SendPort ackPort;
  _FlushCommand(this.ackPort);
}

class _ShutdownCommand {
  final SendPort ackPort;
  _ShutdownCommand(this.ackPort);
}

// Blocking C read/write/close bindings: dart:io can't adopt a pre-existing fd.
typedef NativeRead = Long Function(Int32 fd, Pointer<Uint8> buf, Size count);
typedef DartRead = int Function(int fd, Pointer<Uint8> buf, int count);

typedef NativeWrite = Long Function(Int32 fd, Pointer<Uint8> buf, Size count);
typedef DartWrite = int Function(int fd, Pointer<Uint8> buf, int count);

typedef NativeClose = Int32 Function(Int32 fd);
typedef DartClose = int Function(int fd);

// On Windows the native side returns a CRT fd (via _open_osfhandle), so the
// underscore-prefixed ucrtbase symbols are used; on POSIX it's a real socket fd.
final DynamicLibrary _libc = Platform.isWindows
    ? DynamicLibrary.open('ucrtbase.dll')
    : DynamicLibrary.process();

final DartRead _cRead = _libc.lookupFunction<NativeRead, DartRead>(
  Platform.isWindows ? '_read' : 'read',
);
final DartWrite _cWrite = _libc.lookupFunction<NativeWrite, DartWrite>(
  Platform.isWindows ? '_write' : 'write',
);
final DartClose _cClose = _libc.lookupFunction<NativeClose, DartClose>(
  Platform.isWindows ? '_close' : 'close',
);

// Resolves the C `errno` address; the symbol differs per platform, null if not
// found.
final Pointer<Int32> Function()? _getErrnoPtr = () {
  try {
    if (Platform.isWindows) {
      return _libc.lookupFunction<Pointer<Int32> Function(),
          Pointer<Int32> Function()>('_errno');
    } else if (Platform.isMacOS || Platform.isIOS) {
      return _libc.lookupFunction<Pointer<Int32> Function(),
          Pointer<Int32> Function()>('__error');
    } else {
      return _libc.lookupFunction<Pointer<Int32> Function(),
          Pointer<Int32> Function()>('__errno_location');
    }
  } catch (_) {
    return null;
  }
}();

// errno is thread-local: read it on the same isolate that made the syscall.
// EINTR is 4 on all supported targets.
bool _isEintr() => _getErrnoPtr != null && _getErrnoPtr!().value == 4;

/// An [SSHSocket] over a raw, already-connected fd from the native side.
///
/// [_cRead]/[_cWrite] block the calling thread, so each direction runs in its
/// own [Isolate] to keep the main event loop free:
///  * read isolate: reads one chunk per request (pull-based for backpressure),
///    forwarding it (`null` = EOF, `String` = error) over a [SendPort];
///  * write isolate: hands back a command [SendPort], then `write()`s chunks,
///    reports write errors, and honours flush/shutdown commands.
///
/// [close] flushes pending writes before tearing down; [destroy] tears down now.
class RawFdSSHSocket implements SSHSocket {
  final int fd;

  // Inbound bytes, exposed via [stream]. Pull-based: the next chunk is only
  // requested when there is unpaused demand, so a slow consumer applies
  // backpressure to the socket.
  late final StreamController<Uint8List> _readController =
      StreamController<Uint8List>(
    onListen: _onReadDemand,
    onResume: _onReadDemand,
    onCancel: _stopReading,
  );
  // Outbound bytes, accepted via [sink]. Typed List<int> to match the interface;
  // normalised to Uint8List before crossing to the write isolate.
  final StreamController<List<int>> _writeController =
      StreamController<List<int>>();

  final ReceivePort _readReceivePort = ReceivePort();
  // Receives the write isolate's command port and any write errors.
  final ReceivePort _writeHandshakePort = ReceivePort();

  late final Future<Isolate> _readIsolateFuture;
  late final Future<Isolate> _writeIsolateFuture;
  final Completer<SendPort> _writePortCompleter = Completer<SendPort>();
  // Completes with the read isolate's request port; used to pull each chunk.
  final Completer<SendPort> _readPortCompleter = Completer<SendPort>();
  bool _readRequestInFlight = false;

  StreamSubscription<List<int>>? _writeSubscription;
  bool _isClosed = false;

  RawFdSSHSocket(this.fd) {
    _initReadIsolate();
    _initWriteIsolate();
  }

  @override
  Stream<Uint8List> get stream => _readController.stream;

  @override
  StreamSink<List<int>> get sink => _writeController.sink;

  @override
  Future<void> get done => _writeController.done;

  // Read isolate: stores its request port, pipes chunks into [_readController]
  // (Uint8List = data, String = error, null = EOF), and pulls the next chunk
  // while there is unpaused demand.
  void _initReadIsolate() {    _readReceivePort.listen((message) {
      if (message is SendPort) {
        if (!_readPortCompleter.isCompleted)
          _readPortCompleter.complete(message);
        return;
      }

      _readRequestInFlight = false;
      if (message is Uint8List) {
        _readController.add(message);
        _onReadDemand(); // Pull the next chunk unless paused.
      } else if (message is String) {
        _readController.addError(SocketException(message));
        if (!_readController.isClosed) _readController.close();
      } else if (message == null) {
        if (!_readController.isClosed) _readController.close();
      }
    });

    _readIsolateFuture = Isolate.spawn(
      _readWorkerLoop,
      [_readReceivePort.sendPort, fd],
    );
  }

  // Requests one chunk from the read isolate, but only with unpaused demand and
  // no request already outstanding — this is the backpressure valve.
  void _onReadDemand() {
    if (_isClosed || _readRequestInFlight || _readController.isPaused) return;
    _readRequestInFlight = true;
    _readPortCompleter.future.then((port) {
      if (!_isClosed) port.send('read');
    }).catchError((_) {});
  }

  // Tells the read isolate to stop and release its buffer.
  void _stopReading() {
    _readPortCompleter.future
        .then((port) => port.send('stop'))
        .catchError((_) {});
  }

  // Surfaces a write failure on the read stream, then tears down.
  void _handleWriteError(String message) {
    if (_isClosed) return;
    if (!_readController.isClosed) {
      _readController.addError(SocketException(message));
    }
    destroy();
  }

  // Write isolate: after the handshake, forwards outbound chunks to its port.
  void _initWriteIsolate() async {
    _writeHandshakePort.listen((message) {
      if (message is SendPort) {
        if (!_writePortCompleter.isCompleted)
          _writePortCompleter.complete(message);
      } else if (message is String) {
        _handleWriteError(message);
      }
    });

    _writeIsolateFuture = Isolate.spawn(
      _writeWorkerLoop,
      [_writeHandshakePort.sendPort, fd],
    );

    try {
      final writePort = await _writePortCompleter.future;
      if (_isClosed) return;

      _writeSubscription = _writeController.stream.listen((data) {
        if (data.isNotEmpty && !_isClosed) {
          // Normalise: the write isolate only accepts Uint8List.
          writePort.send(data is Uint8List ? data : Uint8List.fromList(data));
        }
      });
    } catch (_) {
      // Aborted by destroy() before handshake; ignore.
    }
  }

  // Read isolate body: reads one chunk per 'read' request (pull-based) and
  // ships it back; 'stop' releases the buffer and ends the isolate.
  static void _readWorkerLoop(List<dynamic> args) {
    final SendPort sendPort = args[0];
    final int fd = args[1];
    const bufferSize = 16384;
    final buffer = calloc<Uint8>(bufferSize);

    late final RawReceivePort requestPort;
    var closed = false;
    void cleanup() {
      if (closed) return;
      closed = true;
      calloc.free(buffer);
      requestPort.close();
    }

    requestPort = RawReceivePort((message) {
      if (message == 'stop') {
        cleanup();
        return;
      }
      // 'read': perform exactly one blocking read.
      while (true) {
        final bytesRead = _cRead(fd, buffer, bufferSize);
        if (bytesRead < 0 && _isEintr()) continue; // Interrupted: retry.
        if (bytesRead == 0) {
          sendPort.send(null); // EOF.
          cleanup();
        } else if (bytesRead < 0) {
          sendPort.send('Native socket read failed with code $bytesRead');
          cleanup();
        } else {
          // Copy out of the shared buffer before sending.
          sendPort.send(Uint8List.fromList(buffer.asTypedList(bytesRead)));
        }
        return;
      }
    });

    // Hand back the request port so the main isolate can pull chunks.
    sendPort.send(requestPort.sendPort);
  }

  // Write isolate body: drain outbound chunks, writing each fully to the fd,
  // reporting a non-recoverable write failure back over the handshake port.
  static void _writeWorkerLoop(List<dynamic> args) {
    final SendPort handshakePort = args[0];
    final int fd = args[1];

    // Lazy so nothing leaks if killed before any write; calloc memory is
    // process-global and survives isolate.kill(), so free it on shutdown.
    Pointer<Uint8> writeBuf = nullptr;
    int bufferCap = 0;

    late final RawReceivePort commandPort;
    commandPort = RawReceivePort((message) {
      if (message is Uint8List) {
        if (message.length > bufferCap) {
          if (writeBuf != nullptr) calloc.free(writeBuf);
          bufferCap = message.length;
          writeBuf = calloc<Uint8>(bufferCap);
        }

        writeBuf.asTypedList(message.length).setAll(0, message);

        // write() may be partial: loop until the whole chunk is out.
        int bytesWritten = 0;
        while (bytesWritten < message.length) {
          final result = _cWrite(
            fd,
            writeBuf + bytesWritten,
            message.length - bytesWritten,
          );
          if (result < 0 && _isEintr()) continue; // Interrupted: retry.
          if (result <= 0) {
            // Error/closed: report the failure and stop draining this chunk.
            handshakePort.send('Native socket write failed with code $result');
            break;
          }
          bytesWritten += result;
        }
      } else if (message is _FlushCommand) {
        // Writes are synchronous here, so an ack means "flushed".
        message.ackPort.send(null);
      } else if (message is _ShutdownCommand) {
        if (writeBuf != nullptr) calloc.free(writeBuf);
        commandPort.close();
        message.ackPort.send(null);
      }
    });

    // Hand back the command port so the main isolate can start writing.
    handshakePort.send(commandPort.sendPort);
  }

  // Graceful: stop new writes, flush queued ones, then destroy.
  @override
  Future<void> close() async {
    await _writeController.close();

    if (!_isClosed && _writePortCompleter.isCompleted) {
      try {
        final writePort = await _writePortCompleter.future;
        final flushAckPort = ReceivePort();
        writePort.send(_FlushCommand(flushAckPort.sendPort));
        await flushAckPort.first;
        flushAckPort.close();
      } catch (_) {}
    }

    destroy();
  }

  // Immediate teardown; idempotent via [_isClosed].
  @override
  void destroy() {
    if (_isClosed) return;
    _isClosed = true;

    _writeSubscription?.cancel();
    _readReceivePort.close();
    _writeHandshakePort.close();

    if (!_readController.isClosed) _readController.close();
    // Complete `done` for anyone awaiting it (e.g. the write-error path never
    // goes through close()).
    if (!_writeController.isClosed) _writeController.close();

    void finalizeTeardown() {
      // Ask the (usually idle) read isolate to release its buffer and exit.
      _stopReading();
      // Shut down first so any blocked read() returns EOF and its isolate
      // exits: isolate.kill() can't interrupt a thread parked in a syscall.
      shutdownSocket(fd);
      _cClose(fd);

      Future.wait([_readIsolateFuture, _writeIsolateFuture]).then((isolates) {
        for (final isolate in isolates) {
          isolate.kill(priority: Isolate.beforeNextEvent);
        }
      });
    }

    if (!_writePortCompleter.isCompleted) {
      // Destroyed before the handshake: fail the completer, silence its
      // unhandled error, then tear down.
      _writePortCompleter.completeError(
        StateError('Socket destroyed before write isolate handshake completed'),
      );
      _writePortCompleter.future.ignore();
      finalizeTeardown();
    } else {
      // Shut the write isolate down cleanly, then finalize once acked.
      final shutdownAckPort = ReceivePort();
      _writePortCompleter.future.then((port) {
        port.send(_ShutdownCommand(shutdownAckPort.sendPort));
        shutdownAckPort.first.then((_) {
          shutdownAckPort.close();
          finalizeTeardown();
        });
      }).catchError((_) {
        finalizeTeardown();
      });
    }
  }
}
