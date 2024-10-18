import 'package:basics/basics.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../platform/platform.dart';
import '../providers.dart';

final hotkeySettingProvider = guiSettingProvider(hotkeyKey);

class HotkeyNotifier extends Notifier<SingleActivator?> {
  @override
  SingleActivator? build() {
    final hotkeyString = ref.read(hotkeySettingProvider);
    if (hotkeyString == null) return null;
    final components = hotkeyString.toLowerCase().split('+');
    final keyId =
        components.map(int.tryParse).firstWhereOrNull((e) => e != null);
    if (keyId == null) return null;
    final key = LogicalKeyboardKey.findKeyByKeyId(keyId);
    if (key == null) return null;
    if (!components.containsAny(['alt', 'control', 'meta'])) return null;

    return SingleActivator(
      key,
      alt: components.contains('alt'),
      control: components.contains('control'),
      meta: components.contains('meta'),
      shift: components.contains('shift'),
      includeRepeats: false,
    );
  }

  void set(SingleActivator? activator) {
    state = activator;
    if (activator == null) {
      ref.read(hotkeySettingProvider.notifier).set('');
      return;
    }

    final hotkeyString = [
      if (activator.alt) 'alt',
      if (activator.control) 'control',
      if (activator.meta) 'meta',
      if (activator.shift) 'shift',
      activator.trigger.keyId.toString(),
    ].join('+');
    ref.read(hotkeySettingProvider.notifier).set(hotkeyString);
  }
}

final hotkeyProvider =
    NotifierProvider<HotkeyNotifier, SingleActivator?>(HotkeyNotifier.new);

class HotkeyRecorder extends StatefulWidget {
  final SingleActivator? value;
  final ValueChanged<SingleActivator?>? onSave;

  const HotkeyRecorder({super.key, this.value, this.onSave});

  @override
  State<HotkeyRecorder> createState() => HotkeyRecorderState();
}

class HotkeyRecorderState extends State<HotkeyRecorder> {
  final focusNode = FocusNode();
  late var hasFocus = focusNode.hasFocus;
  var alt = false;
  var control = false;
  var meta = false;
  var shift = false;
  LogicalKeyboardKey? key;
  var shouldSave = false;

  @override
  void initState() {
    super.initState();
    set(widget.value);
  }

  @override
  void dispose() {
    focusNode.dispose();
    super.dispose();
  }

  void set(SingleActivator? value) {
    alt = value?.alt ?? false;
    control = value?.control ?? false;
    meta = value?.meta ?? false;
    shift = value?.shift ?? false;
    key = value?.trigger;
  }

  void setAlt(bool pressed) => alt = pressed;

  void setCtrl(bool pressed) => control = pressed;

  void setMeta(bool pressed) => meta = pressed;

  void setShift(bool pressed) => shift = pressed;

  late final modifierSetters = {
    LogicalKeyboardKey.altLeft: setAlt,
    LogicalKeyboardKey.altRight: setAlt,
    LogicalKeyboardKey.controlLeft: setCtrl,
    LogicalKeyboardKey.controlRight: setCtrl,
    LogicalKeyboardKey.metaLeft: setMeta,
    LogicalKeyboardKey.metaRight: setMeta,
    LogicalKeyboardKey.superKey: setMeta,
    LogicalKeyboardKey.shiftLeft: setShift,
    LogicalKeyboardKey.shiftRight: setShift,
  };

  KeyEventResult handleKeyEvent(_, KeyEvent event) {
    if (event is KeyRepeatEvent) return KeyEventResult.handled;

    shouldSave = false;
    final pressed = event is KeyDownEvent;
    final logicalKey = event.logicalKey;

    final modifierSetter = modifierSetters[logicalKey];
    if (modifierSetter != null) {
      setState(() => modifierSetter.call(pressed));
    } else {
      setState(() => key = pressed ? logicalKey : null);
    }

    if (key != null && [alt, control, meta].any((pressed) => pressed)) {
      shouldSave = true;
      focusNode.unfocus();
    }

    return KeyEventResult.handled;
  }

  void handleFocusChange(bool focused) {
    setState(() {
      hasFocus = focused;
      if (focused) {
        set(null);
        shouldSave = true;
      } else if (shouldSave) {
        shouldSave = false;
        final trigger = key;
        final activator = trigger != null
            ? SingleActivator(
                trigger,
                alt: alt,
                control: control,
                meta: meta,
                shift: shift,
                includeRepeats: false,
              )
            : null;
        widget.onSave?.call(activator);
      } else {
        set(widget.value);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final keyLabel = key?.keyLabel ?? '...';
    final modifiers = [
      if (control) 'Ctrl',
      if (alt) mpPlatform.altKey,
      if (shift) 'Shift',
      if (meta) mpPlatform.metaKey,
    ].join('+');
    final keyCombination = modifiers.isNotEmpty
        ? '$modifiers+$keyLabel'
        : hasFocus
            ? 'Input...'
            : '';

    return Focus(
      focusNode: focusNode,
      onFocusChange: handleFocusChange,
      onKeyEvent: handleKeyEvent,
      child: TapRegion(
        onTapInside: (_) => focusNode.requestFocus(),
        onTapOutside: (_) => focusNode.unfocus(),
        child: InputDecorator(
          decoration: InputDecoration(contentPadding: EdgeInsets.zero),
          child: Container(
            width: 260,
            height: 42,
            alignment: Alignment.center,
            child: Text(keyCombination),
          ),
        ),
      ),
    );
  }
}
