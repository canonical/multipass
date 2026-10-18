import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/vm_details/mapping_slider.dart';

void main() {
  group('NumToHumanString.toNiceString', () {
    test('integer returns plain int string', () {
      expect(5.toNiceString(), '5');
      expect(0.toNiceString(), '0');
      expect(100.toNiceString(), '100');
    });

    test('double with fractional part returns 2 decimal places', () {
      expect(1.5.toNiceString(), '1.50');
      expect(3.14.toNiceString(), '3.14');
      expect(0.1.toNiceString(), '0.10');
      expect((0.101).toNiceString(), '0.10');
      expect((0.100).toNiceString(), '0.10');
    });

    test('double equal to int returns int string', () {
      expect(2.0.toNiceString(), '2');
      expect(0.0.toNiceString(), '0');
      expect(10.0.toNiceString(), '10');
    });

    test('double that rounds to an int returns int string', () {
      expect(0.001.toNiceString(), '0');
      expect(1.999.toNiceString(), '2');
      expect((-0.001).toNiceString(), '0');
    });
  });

  group('BytesFromUnits', () {
    for (final (expression, actual, expected) in [
      ('0.kibi', 0.kibi, 0),
      ('1.kibi', 1.kibi, 1024),
      ('2.kibi', 2.kibi, 2048),
      ('1.mebi', 1.mebi, 1048576),
      ('1.gibi', 1.gibi, 1073741824),
    ]) {
      test('$expression equals $expected', () {
        expect(actual, expected);
      });
    }
  });

  group('Conversion functions', () {
    for (final (unit, fromBytes, toBytes, bytesPerUnit) in [
      ('B', bytesToBytes, bytesToBytes, 1),
      ('KiB', bytesToKibi, kibiToBytes, 1.kibi),
      ('MiB', bytesToMebi, mebiToBytes, 1.mebi),
      ('GiB', bytesToGibi, gibiToBytes, 1.gibi),
    ]) {
      test('converts between bytes and $unit', () {
        expect(fromBytes(bytesPerUnit), 1);
        expect(toBytes(1), bytesPerUnit);
        expect(toBytes(1.5), 1.5 * bytesPerUnit);
      });
    }
  });

  group('nonLinearMapping and nonLinearInverseMapping', () {
    for (final value in [
      512,
      1.mebi,
      1.gibi,
      1.5.gibi,
      3.gibi,
      8.gibi,
      1024.gibi,
    ]) {
      test('round-trips $value bytes', () {
        expect(nonLinearInverseMapping(nonLinearMapping(value)), value);
      });
    }

    test('advances 8 positions each time the value doubles', () {
      for (var value = 512; value <= 1024.gibi; value *= 2) {
        expect(nonLinearMapping(2 * value), nonLinearMapping(value) + 8);
      }
    });

    test('spaces positions evenly between consecutive powers of two', () {
      final start = nonLinearMapping(1.gibi);
      for (var step = 0; step <= 8; step++) {
        expect(nonLinearInverseMapping(start + step), 1.gibi + step * 128.mebi);
      }
    });

    test('rounds values between positions down to the lower position', () {
      for (final value in [1.gibi + 1, 1.gibi + 128.mebi - 1]) {
        expect(nonLinearInverseMapping(nonLinearMapping(value)), 1.gibi);
      }
    });

    test('maps each position back to itself', () {
      // Below position 24, a doubling spans fewer than 8 sectors.
      for (var position = 24; position < 8 * 40; position++) {
        expect(nonLinearMapping(nonLinearInverseMapping(position)), position);
      }
    });

    test('inverseMapping produces multiples of sector size', () {
      const sectorSize = 512;
      for (var i = 0; i < 20; i++) {
        expect(nonLinearInverseMapping(i) % sectorSize, 0);
      }
    });

    test('does not throw for values below the sector size', () {
      for (final value in [0, 100, 511]) {
        expect(() => nonLinearMapping(value), returnsNormally);
      }
    });

    test('maps sub-sector values to the minimum slider position', () {
      expect(nonLinearMapping(0), 0);
      expect(nonLinearMapping(511), 0);
    });
  });
}
