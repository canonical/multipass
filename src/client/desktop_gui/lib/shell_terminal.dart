import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_pty/flutter_pty.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:xterm/xterm.dart';

import 'settings.dart';

const terminalFontSizeKey = 'terminalFontSize';

class ShellTerminal extends ConsumerStatefulWidget {
  final String instance;

  const ShellTerminal({super.key, required this.instance});

  @override
  ConsumerState<ShellTerminal> createState() => _ShellTerminalState();
}

class _ShellTerminalState extends ConsumerState<ShellTerminal> {
  final terminal = Terminal(maxLines: 10000);
  final terminalController = TerminalController();

  late double fontSize =
      ref.read(settingsProvider).getDouble(terminalFontSizeKey) ?? 13;

  void setFontSize(double size) {
    size = size.clamp(5, 25);
    setState(() => fontSize = size);
    ref.read(settingsProvider).setDouble(terminalFontSizeKey, size);
  }

  void increaseFontSize() => setFontSize(fontSize + 0.5);

  void decreaseFontSize() => setFontSize(fontSize - 0.5);

  void resetFontSize() => setFontSize(13);

  late final shortcuts = {
    increaseFontSize.intent: [
      const SingleActivator(LogicalKeyboardKey.numpadAdd, control: true),
      const SingleActivator(LogicalKeyboardKey.equal, control: true),
      const SingleActivator(LogicalKeyboardKey.add, control: true, shift: true),
    ],
    decreaseFontSize.intent: [
      const SingleActivator(LogicalKeyboardKey.numpadSubtract, control: true),
      const SingleActivator(LogicalKeyboardKey.minus, control: true),
      const SingleActivator(LogicalKeyboardKey.minus,
          control: true, shift: true),
    ],
    resetFontSize.intent: [
      const SingleActivator(LogicalKeyboardKey.digit0, control: true),
    ],
  }.entries.fold(
    // this goes from a map of {Intent1: [Shortcut1, Shortcut2], Intent2: [Shortcut3, Shortcut4]}
    // to one of {Shortcut1: Intent1, Shortcut2: Intent1, Shortcut3: Intent2, Shortcut4: Intent2}
    <SingleActivator, VoidCallbackIntent>{},
    (map, entry) {
      final intent = entry.key;
      final activators = entry.value;
      return {...map, for (final activator in activators) activator: intent};
    },
  );

  Pty openShell() {
    final environment = {...Platform.environment};
    if (Platform.environment['SNAP_NAME'] == 'multipass') {
      environment["XDG_DATA_HOME"] =
          '${Platform.environment['SNAP_USER_DATA']!}/data';
    }

    final executable = Platform.isWindows ? 'cmd' : 'multipass';
    final args = Platform.isWindows
        ? ['/c', 'multipass', 'shell', widget.instance]
        : ['shell', widget.instance];

    return Pty.start(
      executable,
      arguments: args,
      environment: environment,
      columns: terminal.viewWidth,
      rows: terminal.viewHeight,
      ackRead: true,
    );
  }

  @override
  void initState() {
    super.initState();
    final pty = openShell();
    pty.exitCode.then(
      (code) {
        if (code == 0) exit(0);
        terminal.write('Process ended with exit code $code');
      },
    );
    pty.output.listen((data) => terminal.write(utf8.decoder.convert(data)));
    Stream.periodic(Duration.zero).listen((_) => pty.ackRead());
    terminal.onOutput = (data) => pty.write(utf8.encoder.convert(data));
    terminal.onResize = (w, h, _, __) => pty.resize(h, w);
  }

  @override
  Widget build(BuildContext context) {
    return Actions(
      actions: {VoidCallbackIntent: VoidCallbackAction()},
      child: TerminalView(
        terminal,
        controller: terminalController,
        autofocus: true,
        shortcuts: shortcuts,
        hardwareKeyboardOnly: true,
        textStyle: TerminalStyle(fontSize: fontSize),
        onSecondaryTapDown: (_, __) async {
          final selection = terminalController.selection;
          if (selection != null) {
            final text = terminal.buffer.getText(selection);
            terminalController.clearSelection();
            await Clipboard.setData(ClipboardData(text: text));
          } else {
            final data = await Clipboard.getData(Clipboard.kTextPlain);
            final text = data?.text;
            if (text != null) {
              terminal.paste(text);
            }
          }
        },
      ),
    );
  }
}

extension on VoidCallback {
  VoidCallbackIntent get intent => VoidCallbackIntent(this);
}
