import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'sidebar.dart';

final _hoveredLinkProvider = StateProvider.autoDispose<TextSpan?>((ref) {
  ref.watch(sidebarKeyProvider);
  return null;
});

extension TextSpanFromStringExt on String {
  TextSpan get span => TextSpan(
        text: this,
        style: const TextStyle(
          color: Colors.black,
          fontFamily: 'Ubuntu',
          fontFamilyFallback: ['NotoColorEmoji', 'FreeSans'],
        ),
      );
}

extension TextSpanFromListExt on List<TextSpan> {
  TextSpan get spans => TextSpan(
        children: this,
        style: const TextStyle(color: Colors.black),
      );
}

extension TextSpanExt on TextSpan {
  static const noStyle = TextStyle();

  TextSpan get bold => TextSpan(
        text: text,
        children: children,
        style: (style ?? noStyle).copyWith(fontWeight: FontWeight.bold),
      );

  TextSpan size(double size) => TextSpan(
        text: text,
        children: children,
        style: (style ?? noStyle).copyWith(fontSize: size),
      );

  TextSpan color(Color color) => TextSpan(
        text: text,
        children: children,
        style: (style ?? noStyle).copyWith(color: color),
      );

  TextSpan font(String fontFamily) => TextSpan(
        text: text,
        children: children,
        style: (style ?? noStyle).copyWith(fontFamily: fontFamily),
      );

  TextSpan backgroundColor(Color color) => TextSpan(
        text: text,
        children: children,
        style: (style ?? noStyle).copyWith(backgroundColor: color),
      );

  TextSpan link(WidgetRef ref, VoidCallback callback) {
    final hovered = ref.watch(_hoveredLinkProvider) == this;
    return TextSpan(
      text: text,
      children: children,
      style: (style ?? noStyle).copyWith(
        color: hovered ? Colors.blue : null,
        decoration: hovered ? TextDecoration.underline : null,
        decorationColor: Colors.blue,
      ),
      recognizer: TapGestureRecognizer()..onTap = callback,
      onEnter: (_) => ref.read(_hoveredLinkProvider.notifier).state = this,
      onExit: (_) {
        if (ref.context.mounted) ref.invalidate(_hoveredLinkProvider);
      },
    );
  }
}

extension WidgetGap on Iterable<Widget> {
  Iterable<Widget> gap({double? width, double? height}) sync* {
    final thisIterator = iterator;
    final gapBox = SizedBox(width: width, height: height);
    if (thisIterator.moveNext()) {
      yield thisIterator.current;
      while (thisIterator.moveNext()) {
        yield gapBox;
        yield thisIterator.current;
      }
    }
  }
}

extension NonBreakingString on String {
  String get nonBreaking => replaceAll('-', '\u2011').replaceAll(' ', '\u00A0');
}

extension NullableMap<T> on T? {
  U? map<U>(U? Function(T) f) {
    final self = this;
    try {
      return self == null ? null : f(self);
    } catch (_) {
      return null;
    }
  }
}
