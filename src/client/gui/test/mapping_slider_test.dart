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
    });

    test('double equal to int returns int string', () {
      expect(2.0.toNiceString(), '2');
      expect(0.0.toNiceString(), '0');
      expect(10.0.toNiceString(), '10');
    });
  });

  group('BytesFromUnits', () {
    test('1.kibi equals 1024', () {
      expect(1.kibi, 1024);
    });

    test('1.mebi equals 1048576', () {
      expect(1.mebi, 1048576);
    });

    test('1.gibi equals 1073741824', () {
      expect(1.gibi, 1073741824);
    });

    test('2.kibi equals 2048', () {
      expect(2.kibi, 2048);
    });

    test('0.kibi equals 0', () {
      expect(0.kibi, 0);
    });
  });

  group('Conversion functions', () {
    test('bytesToKibi round-trip with kibiToBytes', () {
      final value = 1.kibi;
      expect(kibiToBytes(bytesToKibi(value)), closeTo(value, 1e-9));
    });

    test('bytesToGibi of 1.gibi is approximately 1.0', () {
      expect(bytesToGibi(1.gibi), closeTo(1.0, 1e-9));
    });

    test('gibiToBytes(1) equals 1.gibi', () {
      expect(gibiToBytes(1), 1.gibi);
    });

    test('bytesToMebi round-trip with mebiToBytes', () {
      final value = 1.mebi;
      expect(mebiToBytes(bytesToMebi(value)), closeTo(value, 1e-9));
    });

    test('bytesToBytes is identity', () {
      expect(bytesToBytes(1.gibi), 1.gibi);
    });
  });

  group('nonLinearMapping and nonLinearInverseMapping', () {
    test('round-trip for 1.gibi', () {
      final value = 1.gibi;
      expect(nonLinearInverseMapping(nonLinearMapping(value)), value);
    });

    test('round-trip for 4.gibi', () {
      final value = 4.gibi;
      expect(nonLinearInverseMapping(nonLinearMapping(value)), value);
    });

    test('round-trip for 8.gibi', () {
      final value = 8.gibi;
      expect(nonLinearInverseMapping(nonLinearMapping(value)), value);
    });

    test('larger mapped value for larger input', () {
      expect(nonLinearMapping(4.gibi), greaterThan(nonLinearMapping(1.gibi)));
      expect(nonLinearMapping(8.gibi), greaterThan(nonLinearMapping(4.gibi)));
    });

    test('inverseMapping produces multiples of sector size', () {
      const sectorSize = 512;
      for (var i = 0; i < 20; i++) {
        expect(nonLinearInverseMapping(i) % sectorSize, 0);
      }
    });
  });
}
