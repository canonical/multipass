import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/extensions.dart';

Matcher isGapBox({double? width, double? height}) => isA<SizedBox>()
    .having((b) => b.width, 'width', width)
    .having((b) => b.height, 'height', height);

void main() {
  group('NonBreakingString', () {
    test('replaces hyphens with non-breaking hyphens', () {
      expect('foo-bar'.nonBreaking, 'foo\u2011bar');
    });

    test('replaces spaces with non-breaking spaces', () {
      expect('foo bar'.nonBreaking, 'foo\u00A0bar');
    });

    test('replaces both hyphens and spaces', () {
      expect('foo-bar baz'.nonBreaking, 'foo\u2011bar\u00A0baz');
    });

    test('returns the same string when no hyphens or spaces are present', () {
      expect('foobar'.nonBreaking, 'foobar');
    });

    test('handles empty string', () {
      expect(''.nonBreaking, '');
    });
  });

  group('NullableMap', () {
    test('returns null when the value is null', () {
      String? value;
      expect(value.map((v) => v.length), isNull);
    });

    test('applies the function when the value is non-null', () {
      const String? value = 'hello';
      expect(value.map((v) => v.length), 5);
    });

    test('returns null when the function throws', () {
      const String? value = 'hello';
      expect(value.map<int>((v) => throw Exception('oops')), isNull);
    });
  });

  group('WidgetGap', () {
    for (final count in [0, 1, 2, 3]) {
      test('places a gap between each of $count widgets', () {
        final widgets = [
          for (var i = 0; i < count; i++) SizedBox(key: ValueKey(i)),
        ];

        final result = widgets.gap(width: 10, height: 5).toList();

        expect(result, hasLength(count == 0 ? 0 : 2 * count - 1));
        for (final (i, widget) in result.indexed) {
          if (i.isEven) {
            expect(widget, same(widgets[i ~/ 2]));
          } else {
            expect(widget, isGapBox(width: 10, height: 5));
          }
        }
      });
    }
  });

  group('TextSpanFromStringExt', () {
    test('wraps the string as the span text', () {
      expect('hello'.span.text, 'hello');
    });

    test('applies a default style so text renders consistently', () {
      expect('hello'.span.style, isNotNull);
    });
  });

  group('TextSpanFromListExt', () {
    test('wraps the given spans as children of a single TextSpan', () {
      final children = ['a'.span, 'b'.span];
      final result = children.spans;
      expect(result.children, children);
    });

    test('an empty list produces a span with no text and no children', () {
      final result = <TextSpan>[].spans;
      expect(result.text, isNull);
      expect(result.children, isEmpty);
    });
  });

  group('TextSpanExt', () {
    test('bold applies FontWeight.bold', () {
      final span = 'hello'.span.bold;
      expect(span.style?.fontWeight, FontWeight.bold);
    });

    test('bold preserves existing text', () {
      final span = 'hello'.span.bold;
      expect(span.text, 'hello');
    });

    test('bold preserves children', () {
      final child = 'child'.span;
      final parent = TextSpan(children: [child]);
      expect(parent.bold.children, [child]);
    });

    test('size applies the given fontSize', () {
      final span = 'hello'.span.size(20);
      expect(span.style?.fontSize, 20);
    });

    test('size preserves existing text', () {
      final span = 'hello'.span.size(16);
      expect(span.text, 'hello');
    });

    test('color applies the given color', () {
      final span = 'hello'.span.color(Colors.red);
      expect(span.style?.color, Colors.red);
    });

    test('color preserves existing text', () {
      final span = 'hello'.span.color(Colors.red);
      expect(span.text, 'hello');
    });

    test('font applies the given fontFamily', () {
      final span = 'hello'.span.font('Monospace');
      expect(span.style?.fontFamily, 'Monospace');
    });

    test('font preserves existing text', () {
      final span = 'hello'.span.font('Monospace');
      expect(span.text, 'hello');
    });

    test('backgroundColor applies the given background color', () {
      final span = 'hello'.span.backgroundColor(Colors.yellow);
      expect(span.style?.backgroundColor, Colors.yellow);
    });

    test('backgroundColor preserves existing text', () {
      final span = 'hello'.span.backgroundColor(Colors.yellow);
      expect(span.text, 'hello');
    });

    test('chaining bold and size applies both styles', () {
      final span = 'hello'.span.bold.size(18);
      expect(span.style?.fontWeight, FontWeight.bold);
      expect(span.style?.fontSize, 18);
    });

    test('chaining color and font applies both styles', () {
      final span = 'hello'.span.color(Colors.green).font('Serif');
      expect(span.style?.color, Colors.green);
      expect(span.style?.fontFamily, 'Serif');
    });

    test('bold on span with no existing style still applies bold', () {
      const span = TextSpan(text: 'plain');
      expect(span.bold.style?.fontWeight, FontWeight.bold);
    });

    test('size on span with no existing style still applies size', () {
      const span = TextSpan(text: 'plain');
      expect(span.size(14).style?.fontSize, 14);
    });
  });
}
