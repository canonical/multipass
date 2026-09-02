import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/extensions.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/vm_table/vm_table_headers.dart';

void main() {
  group('ZONE table header', () {
    final zoneHeader = headers.singleWhere((header) => header.name == 'ZONE');

    VmInfo buildVmInfo({
      required String zoneName,
      required bool supported,
    }) {
      return VmInfo(
        zone: Zone(name: zoneName, supported: supported),
      );
    }

    test('is present in the headers list', () {
      expect(zoneHeader.name, 'ZONE');
    });

    test('sortKey returns the zone name when supported', () {
      final info = buildVmInfo(zoneName: 'eu west-1', supported: true);

      expect(zoneHeader.sortKey!(info), 'eu west-1');
    });

    test('sortKey falls back to n/a when unsupported', () {
      final info = buildVmInfo(zoneName: 'ignored', supported: false);

      expect(zoneHeader.sortKey!(info), 'n/a');
    });

    test('cellBuilder renders the supported zone name with non-breaking text',
        () {
      final info = buildVmInfo(zoneName: 'eu west-1', supported: true);

      final text = zoneHeader.cellBuilder(info) as Text;

      expect(text.data, 'eu west-1'.nonBreaking);
      expect(text.data, contains('\u00A0'));
      expect(text.data, contains('\u2011'));
    });

    test('cellBuilder renders n/a when the zone is unsupported', () {
      final info = buildVmInfo(zoneName: 'ignored', supported: false);

      final text = zoneHeader.cellBuilder(info) as Text;

      expect(text.data, 'n/a');
    });
  });
}
