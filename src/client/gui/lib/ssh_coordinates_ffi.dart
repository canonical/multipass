import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';

import 'generated/multipass.pb.dart';

const int vsockNone = 0;
const int vsockHvsock = 1;
const int vsockVsock = 2;
const int vsockUsock = 3;

final class VsockUnion extends ffi.Union {
  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> hvsock_vmid;

  @ffi.Uint32()
  // ignore: non_constant_identifier_names
  external int vsock_cid;

  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> usock_addr;
}

final class FfiSSHCoordinates extends ffi.Struct {
  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> username;

  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> private_key_as_base64;

  @ffi.Uint32()
  // ignore: non_constant_identifier_names
  external int port;

  // ignore: non_constant_identifier_names
  external ffi.Pointer<Utf8> tcp_host;

  @ffi.Uint32()
  // ignore: non_constant_identifier_names
  external int vsock_tag;

  // ignore: non_constant_identifier_names
  external VsockUnion vsock;
}

ffi.Pointer<FfiSSHCoordinates> sshCoordinatesInfoToFfi(
    SSHCoordinatesInfo? proto) {
  if (proto == null) {
    return ffi.nullptr;
  }
  final native = malloc<FfiSSHCoordinates>();

  native.ref.username = proto.username.toNativeUtf8();
  native.ref.private_key_as_base64 = proto.privKeyBase64.toNativeUtf8();
  native.ref.port = proto.port;
  native.ref.tcp_host = proto.tcpHost.toNativeUtf8();

  switch (proto.whichVsockHost()) {
    case SSHCoordinatesInfo_VsockHost.hvsockVmid:
      native.ref.vsock_tag = vsockHvsock;
      native.ref.vsock.hvsock_vmid = proto.hvsockVmid.toNativeUtf8();
    case SSHCoordinatesInfo_VsockHost.vsockCid:
      native.ref.vsock_tag = vsockVsock;
      native.ref.vsock.vsock_cid = proto.vsockCid;
    case SSHCoordinatesInfo_VsockHost.usockAddr:
      native.ref.vsock_tag = vsockUsock;
      native.ref.vsock.usock_addr = proto.usockAddr.toNativeUtf8();
    case SSHCoordinatesInfo_VsockHost.notSet:
      native.ref.vsock_tag = vsockNone;
  }

  return native;
}

void freeFfiSSHCoordinates(ffi.Pointer<FfiSSHCoordinates> p) {
  malloc.free(p.ref.username);
  malloc.free(p.ref.private_key_as_base64);
  malloc.free(p.ref.tcp_host);

  switch (p.ref.vsock_tag) {
    case vsockHvsock:
      malloc.free(p.ref.vsock.hvsock_vmid);
    case vsockUsock:
      malloc.free(p.ref.vsock.usock_addr);
    case vsockVsock:
    case vsockNone:
      break;
    default:
      throw StateError('Unexpected vsock tag: ${p.ref.vsock_tag}');
  }

  malloc.free(p);
}
