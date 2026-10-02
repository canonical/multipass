import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/extensions.dart';

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

    test('works with integer values', () {
      const int? value = 42;
      expect(value.map((v) => v * 2), 84);
    });

    test('returns null for null integer', () {
      int? value;
      expect(value.map((v) => v * 2), isNull);
    });
  });

  group('WidgetGap', () {
    test('empty iterable produces no elements', () {
      final result = <Widget>[].gap(width: 8).toList();
      expect(result, isEmpty);
    });

    test('single element produces no gaps', () {
      final widgets = [const SizedBox()];
      final result = widgets.gap(width: 8).toList();
      expect(result, hasLength(1));
    });

    test('two elements produce one gap between them', () {
      final a = const SizedBox(key: ValueKey('a'));
      final b = const SizedBox(key: ValueKey('b'));
      final result = [a, b].gap(width: 8).toList();
      expect(result, hasLength(3));
      expect(result[0], a);
      expect(result[2], b);
      final gap = result[1] as SizedBox;
      expect(gap.width, 8);
    });

    test('three elements produce two gaps', () {
      final widgets = [
        const SizedBox(key: ValueKey('a')),
        const SizedBox(key: ValueKey('b')),
        const SizedBox(key: ValueKey('c')),
      ];
      final result = widgets.gap(height: 4).toList();
      expect(result, hasLength(5));
      final gap1 = result[1] as SizedBox;
      final gap2 = result[3] as SizedBox;
      expect(gap1.height, 4);
      expect(gap2.height, 4);
    });

    test('gap SizedBox uses specified width and height', () {
      final result = [const SizedBox(), const SizedBox()]
          .gap(width: 10, height: 5)
          .toList();
      final gap = result[1] as SizedBox;
      expect(gap.width, 10);
      expect(gap.height, 5);
    });
  });

  group('TextSpanFromStringExt', () {
    test('span has the correct text', () {
      expect('hello'.span.text, 'hello');
    });

    test('span has black color', () {
      expect('hello'.span.style?.color, Colors.black);
    });

    test('span has Ubuntu font family', () {
      expect('hello'.span.style?.fontFamily, 'Ubuntu');
    });

    test('span has emoji fallback fonts', () {
      expect('hello'.span.style?.fontFamilyFallback,
          containsAllInOrder(['NotoColorEmoji', 'FreeSans']));
    });
  });

  group('TextSpanFromListExt', () {
    test('spans wraps children in a TextSpan', () {
      final children = ['a'.span, 'b'.span];
      final result = children.spans;
      expect(result.children, children);
    });

    test('spans has black color', () {
      final result = ['a'.span].spans;
      expect(result.style?.color, Colors.black);
    });

    test('spans with empty list has no text', () {
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
