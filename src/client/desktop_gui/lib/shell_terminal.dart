import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_pty/flutter_pty.dart';
import 'package:xterm/xterm.dart';

class ShellTerminal extends StatefulWidget {
  final String instance;

  const ShellTerminal({super.key, required this.instance});

  @override
  State<ShellTerminal> createState() => _ShellTerminalState();
}

class _ShellTerminalState extends State<ShellTerminal> {
  final terminal = Terminal(maxLines: 10000);
  final terminalController = TerminalController();

  @override
  void initState() {
    super.initState();
    final pty = Pty.start(
      'multipass',
      arguments: ['shell', widget.instance],
      environment: {
        if (Platform.environment['SNAP_NAME'] == 'multipass')
          "XDG_DATA_HOME": '${Platform.environment['SNAP_USER_DATA']!}/data',
      },
      columns: terminal.viewWidth,
      rows: terminal.viewHeight,
      ackRead: true,
    );
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
    return TerminalView(
      terminal,
      controller: terminalController,
      autofocus: true,
      shortcuts: const {},
      hardwareKeyboardOnly: true,
      onSecondaryTapDown: (details, offset) async {
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
    );
  }
}
