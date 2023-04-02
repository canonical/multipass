import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final hoveredLinkProvider = StateProvider<TextSpan?>((ref) => null);

extension TextSpanFromStringExt on String {
  TextSpan get span => TextSpan(
        text: this,
        style: const TextStyle(color: Colors.black),
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

  TextSpan link(VoidCallback callback, WidgetRef ref) => TextSpan(
        text: text,
        children: children,
        style: (style ?? noStyle).copyWith(
          color: Colors.blue,
          decoration: ref.watch(hoveredLinkProvider) == this
              ? TextDecoration.underline
              : null,
        ),
        recognizer: TapGestureRecognizer()..onTap = callback,
        onEnter: (_) => ref.read(hoveredLinkProvider.notifier).state = this,
        onExit: (_) => ref.invalidate(hoveredLinkProvider),
      );
}
