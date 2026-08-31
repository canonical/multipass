import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/generated/multipass.pb.dart';
import 'package:multipass_gui/ffi_ssh_coordinates.dart';

void main() {
  group('sshCoordinatesInfoToFfi', () {
    SSHCoordinatesInfo sshCoordinatesInfo({
      String username = 'ubuntu',
      String privKeyBase64 = 'cHJpdmF0ZS1rZXk=',
      int port = 22,
      String tcpHost = '127.0.0.1',
      String? hvsockVmid,
      int? vsockCid,
      String? usockAddr,
    }) {
      return SSHCoordinatesInfo(
        username: username,
        privKeyBase64: privKeyBase64,
        port: port,
        tcpHost: tcpHost,
        hvsockVmid: hvsockVmid,
        vsockCid: vsockCid,
        usockAddr: usockAddr,
      );
    }

    void expectScalarFields(
      FfiSSHCoordinates native,
      SSHCoordinatesInfo proto,
    ) {
      expect(native.username.toDartString(), proto.username);
      expect(native.privKeyBlob.toDartString(), proto.privKeyBase64);
      expect(native.port, proto.port);
      expect(native.tcpHost.toDartString(), proto.tcpHost);
    }

    test('converts hvsock coordinates', () {
      const vmid = '12345678-1234-1234-1234-123456789abc';
      final proto = sshCoordinatesInfo(hvsockVmid: vmid);
      final native = sshCoordinatesInfoToFfi(proto);

      try {
        final nativeRef = native.ref;
        expect(nativeRef.vsockTag, vsockTagHvsock);
        expect(nativeRef.vsockData.hvsockVmId.toDartString(), vmid);
        expectScalarFields(nativeRef, proto);
      } finally {
        freeFfiSSHCoordinates(native);
      }
    });

    test('converts vsock coordinates', () {
      const cid = 42;
      final proto = sshCoordinatesInfo(vsockCid: cid);
      final native = sshCoordinatesInfoToFfi(proto);

      try {
        final nativeRef = native.ref;
        expect(nativeRef.vsockTag, vsockTagVsock);
        expect(nativeRef.vsockData.vsockCID, cid);
        expectScalarFields(nativeRef, proto);
      } finally {
        freeFfiSSHCoordinates(native);
      }
    });

    test('converts usock coordinates', () {
      const addr = '/var/snap/multipass/common/multipass_socket';
      final proto = sshCoordinatesInfo(usockAddr: addr);
      final native = sshCoordinatesInfoToFfi(proto);

      try {
        final nativeRef = native.ref;
        expect(nativeRef.vsockTag, vsockTagUsock);
        expect(nativeRef.vsockData.usockAddr.toDartString(), addr);
        expectScalarFields(nativeRef, proto);
      } finally {
        freeFfiSSHCoordinates(native);
      }
    });

    test('uses none tag when vsock host is not set', () {
      final proto = sshCoordinatesInfo();
      final native = sshCoordinatesInfoToFfi(proto);

      try {
        expect(native.ref.vsockTag, vsockTagNone);
        expectScalarFields(native.ref, proto);
      } finally {
        freeFfiSSHCoordinates(native);
      }
    });

    test('returns nullptr for null coordinates', () {
      expect(sshCoordinatesInfoToFfi(null), ffi.nullptr);
    });
  });
}
