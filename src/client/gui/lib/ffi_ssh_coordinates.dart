import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';

import 'generated/multipass.pb.dart';

const int vsockTagNone = 0;
const int vsockTagHvsock = 1;
const int vsockTagVsock = 2;
const int vsockTagUsock = 3;

final class VsockDataUnion extends ffi.Union {
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

  external VsockDataUnion vsockData;
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
      native.ref.vsockTag = vsockTagHvsock;
      native.ref.vsockData.hvsockVmId = proto.hvsockVmid.toNativeUtf8();
      break;
    case SSHCoordinatesInfo_VsockHost.vsockCid:
      native.ref.vsockTag = vsockTagVsock;
      native.ref.vsockData.vsockCID = proto.vsockCid;
      break;
    case SSHCoordinatesInfo_VsockHost.usockAddr:
      native.ref.vsockTag = vsockTagUsock;
      native.ref.vsockData.usockAddr = proto.usockAddr.toNativeUtf8();
      break;
    case SSHCoordinatesInfo_VsockHost.notSet:
      native.ref.vsockTag = vsockTagNone;
      break;
  }

  return native;
}

void freeFfiSSHCoordinates(ffi.Pointer<FfiSSHCoordinates> p) {
  malloc.free(p.ref.username);
  malloc.free(p.ref.privKeyBlob);
  malloc.free(p.ref.tcpHost);

  switch (p.ref.vsockTag) {
    case vsockTagHvsock:
      malloc.free(p.ref.vsockData.hvsockVmId);
      break;
    case vsockTagUsock:
      malloc.free(p.ref.vsockData.usockAddr);
      break;
    case vsockTagVsock:
    case vsockTagNone:
      break;
  }

  malloc.free(p);
}
