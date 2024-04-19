import 'dart:convert';
import 'dart:isolate';
import 'dart:math';

import 'package:async/async.dart';
import 'package:dartssh2/dartssh2.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:xterm/xterm.dart';

import '../logger.dart';
import '../providers.dart';

final vmShellsProvider = StateProvider.autoDispose.family<int, String>((_, __) {
  return 0;
});

class VmTerminal extends ConsumerStatefulWidget {
  final String name;
  final bool running;

  const VmTerminal(this.name, {this.running = false, super.key});

  @override
  ConsumerState<VmTerminal> createState() => VmTerminalState();
}

class VmTerminalState extends ConsumerState<VmTerminal> {
  Terminal? terminal;
  Isolate? isolate;
  final scrollController = ScrollController();
  final focusNode = FocusNode();

  @override
  void initState() {
    super.initState();
    if (widget.running) {
      terminal = Terminal(maxLines: 10000);
      initTerminal();
    }
  }

  @override
  void dispose() {
    isolate?.kill(priority: Isolate.immediate);
    scrollController.dispose();
    focusNode.dispose();
    super.dispose();
  }

  Future<void> initTerminal() async {
    final vmShellsNotifier = ref.read(vmShellsProvider(widget.name).notifier);
    final thisTerminal = terminal;
    if (thisTerminal == null) return;

    final sshInfo = await ref.read(grpcClientProvider).sshInfo(widget.name);
    if (sshInfo == null) return;

    final receiver = ReceivePort();
    final errorReceiver = ReceivePort();
    final exitReceiver = ReceivePort();
    isolate = await Isolate.spawn(
      sshIsolate,
      SshShellInfo(
        sender: receiver.sendPort,
        width: thisTerminal.viewWidth,
        height: thisTerminal.viewHeight,
        sshInfo: sshInfo,
      ),
      onError: errorReceiver.sendPort,
      onExit: exitReceiver.sendPort,
      errorsAreFatal: true,
    );

    vmShellsNotifier.update((state) => state + 1);

    errorReceiver.listen((es) {
      logger.e(
        'Error from ${widget.name} ssh isolate',
        error: es[0],
        stackTrace: es[1] != null ? StackTrace.fromString(es[1]) : null,
      );
    });
    exitReceiver.listen((_) {
      logger.d('Exited ${widget.name} ssh isolate');
      receiver.close();
      errorReceiver.close();
      exitReceiver.close();
      vmShellsNotifier.update((state) => max(0, state - 1));
      if (mounted) setState(() => terminal = null);
    });
    receiver.listen((event) {
      switch (event) {
        case final SendPort sender:
          thisTerminal.onOutput = sender.send;
          thisTerminal.onResize = (w, h, pw, ph) => sender.send([w, h, pw, ph]);
        case final String data:
          thisTerminal.write(data);
        case null:
          logger.i('Ssh session for ${widget.name} has exited');
          isolate?.kill(priority: Isolate.immediate);
      }
    });
    focusNode.requestFocus();
  }

  @override
  Widget build(BuildContext context) {
    final vmRunning = ref.watch(vmInfoProvider(widget.name).select((info) {
      return info.instanceStatus.status == Status.RUNNING;
    }));

    final buttonStyle = ButtonStyle(
      foregroundColor: MaterialStateColor.resolveWith((states) {
        final disabled = states.contains(MaterialState.disabled);
        return Colors.white.withOpacity(disabled ? 0.5 : 1.0);
      }),
      side: MaterialStateBorderSide.resolveWith((states) {
        final disabled = states.contains(MaterialState.disabled);
        var color = Colors.white.withOpacity(disabled ? 0.5 : 1.0);
        return BorderSide(color: color);
      }),
    );

    final thisTerminal = terminal;
    if (thisTerminal == null) {
      return Container(
        color: const Color(0xff380c2a),
        alignment: Alignment.center,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            SvgPicture.asset('assets/shell.svg'),
            const SizedBox(height: 12),
            OutlinedButton(
              style: buttonStyle,
              onPressed: !vmRunning
                  ? null
                  : () => setState(() {
                        terminal = Terminal(maxLines: 10000);
                        initTerminal();
                      }),
              child: const Text('Open shell'),
            ),
            const SizedBox(height: 32),
          ],
        ),
      );
    }

    return RawScrollbar(
      controller: scrollController,
      thickness: 9,
      child: ClipRect(
        child: TerminalView(
          thisTerminal,
          scrollController: scrollController,
          autofocus: true,
          focusNode: focusNode,
          shortcuts: const {},
          hardwareKeyboardOnly: true,
          padding: const EdgeInsets.all(4),
          theme: const TerminalTheme(
            cursor: Color(0xFFE5E5E5),
            selection: Color(0x80E5E5E5),
            foreground: Color(0xffffffff),
            background: Color(0xff380c2a),
            black: Color(0xFF000000),
            white: Color(0xFFE5E5E5),
            red: Color(0xFFCD3131),
            green: Color(0xFF0DBC79),
            yellow: Color(0xFFE5E510),
            blue: Color(0xFF2472C8),
            magenta: Color(0xFFBC3FBC),
            cyan: Color(0xFF11A8CD),
            brightBlack: Color(0xFF666666),
            brightRed: Color(0xFFF14C4C),
            brightGreen: Color(0xFF23D18B),
            brightYellow: Color(0xFFF5F543),
            brightBlue: Color(0xFF3B8EEA),
            brightMagenta: Color(0xFFD670D6),
            brightCyan: Color(0xFF29B8DB),
            brightWhite: Color(0xFFFFFFFF),
            searchHitBackground: Color(0XFFFFFF2B),
            searchHitBackgroundCurrent: Color(0XFF31FF26),
            searchHitForeground: Color(0XFF000000),
          ),
        ),
      ),
    );
  }
}

class SshShellInfo {
  final SendPort sender;
  final int width;
  final int height;
  final SSHInfo sshInfo;

  SshShellInfo({
    required this.sender,
    required this.width,
    required this.height,
    required this.sshInfo,
  });
}

Future<void> sshIsolate(SshShellInfo info) async {
  final SshShellInfo(:sender, :width, :height, :sshInfo) = info;
  final receiver = ReceivePort();
  sender.send(receiver.sendPort);

  final pem = SSHPem.decode(sshInfo.privKeyBase64);
  final rsa = RsaKeyPair.decode(pem);

  final socket = await SSHSocket.connect(sshInfo.host, sshInfo.port);

  final client = SSHClient(
    socket,
    username: sshInfo.username,
    identities: [rsa.getPrivateKeys()],
  );

  final session = await client.shell(
    pty: SSHPtyConfig(width: width, height: height),
  );

  Future.any([client.done, session.done, socket.done]).then((_) {
    sender.send(null);
    receiver.close();
  });

  receiver.listen((event) {
    switch (event) {
      case final String data:
        session.write(utf8.encoder.convert(data));
      case [final int w, final int h, final int pw, final int ph]:
        session.resizeTerminal(w, h, pw, ph);
    }
  });

  StreamGroup.merge([session.stdout, session.stderr]).listen((event) {
    sender.send(utf8.decode(event, allowMalformed: true));
  });
}
