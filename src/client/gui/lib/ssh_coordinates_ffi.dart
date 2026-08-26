import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';

import 'generated/multipass.pb.dart';

const int vsockNone = 0;
const int vsockHvsock = 1;
const int vsockVsock = 2;
const int vsockUsock = 3;

final class VsockUnion extends ffi.Union {
  external ffi.Pointer<Utf8> hvsockVmId;

  @ffi.Uint32()
  external int vsockCID;

  external ffi.Pointer<Utf8> usockAddr;
}

final class FfiSSHCoordinates extends ffi.Struct {
  external ffi.Pointer<Utf8> username;

  external ffi.Pointer<Utf8> privKeyBlob;

  @ffi.Uint32()
  external int port;

  external ffi.Pointer<Utf8> tcpHost;

  @ffi.Uint32()
  external int vsockTag;

  external VsockUnion vsockData;
}

ffi.Pointer<FfiSSHCoordinates> sshCoordinatesInfoToFfi(
    SSHCoordinatesInfo? proto) {
  if (proto == null) {
    return ffi.nullptr;
  }
  final native = malloc<FfiSSHCoordinates>();

  native.ref.username = proto.username.toNativeUtf8();
  native.ref.privKeyBlob = proto.privKeyBase64.toNativeUtf8();
  native.ref.port = proto.port;
  native.ref.tcpHost = proto.tcpHost.toNativeUtf8();

  switch (proto.whichVsockHost()) {
    case SSHCoordinatesInfo_VsockHost.hvsockVmid:
      native.ref.vsockTag = vsockHvsock;
      native.ref.vsockData.hvsockVmId = proto.hvsockVmid.toNativeUtf8();
      break;
    case SSHCoordinatesInfo_VsockHost.vsockCid:
      native.ref.vsockTag = vsockVsock;
      native.ref.vsockData.vsockCID = proto.vsockCid;
      break;
    case SSHCoordinatesInfo_VsockHost.usockAddr:
      native.ref.vsockTag = vsockUsock;
      native.ref.vsockData.usockAddr = proto.usockAddr.toNativeUtf8();
      break;
    case SSHCoordinatesInfo_VsockHost.notSet:
      native.ref.vsockTag = vsockNone;
      break;
  }

  return native;
}

void freeFfiSSHCoordinates(ffi.Pointer<FfiSSHCoordinates> p) {
  malloc.free(p.ref.username);
  malloc.free(p.ref.privKeyBlob);
  malloc.free(p.ref.tcpHost);

  switch (p.ref.vsockTag) {
    case vsockHvsock:
      malloc.free(p.ref.vsockData.hvsockVmId);
    case vsockUsock:
      malloc.free(p.ref.vsockData.usockAddr);
    case vsockVsock:
    case vsockNone:
      break;
    default:
      throw StateError('Unexpected vsock tag: ${p.ref.vsockTag}');
  }

  malloc.free(p);
}
