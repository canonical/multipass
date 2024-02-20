import 'dart:convert';
import 'dart:isolate';

import 'package:async/async.dart';
import 'package:dartssh2/dartssh2.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:xterm/xterm.dart';

import '../providers.dart';

class VmTerminal extends ConsumerStatefulWidget {
  final String name;

  const VmTerminal(this.name, {super.key});

  @override
  ConsumerState<VmTerminal> createState() => _VmTerminalState();
}

class _VmTerminalState extends ConsumerState<VmTerminal> {
  Terminal? terminal;

  Future<void> initTerminal() async {
    final thisTerminal = terminal;
    if (thisTerminal == null) return;

    final sshInfo = await ref.read(grpcClientProvider).sshInfo(widget.name);
    if (sshInfo == null) return;

    final receiver = ReceivePort();
    final errorReceiver = ReceivePort();
    final exitReceiver = ReceivePort();
    final isolate = await Isolate.spawn(
      sshIsolate,
      SshShellInfo(
        sender: receiver.sendPort,
        width: thisTerminal.viewWidth,
        height: thisTerminal.viewHeight,
        sshInfo: sshInfo,
      ),
      debugName: '${widget.name}-ssh-session',
      onError: errorReceiver.sendPort,
      onExit: exitReceiver.sendPort,
      errorsAreFatal: true,
    );
    errorReceiver.listen((message) => print('ON-ERROR: $message'));
    exitReceiver.listen((message) {
      print('ON-EXIT: $message');
      isolate.kill(priority: Isolate.immediate);
      setState(() => terminal = null);
    });

    final receiverStreamQueue = StreamQueue(receiver);
    final sender = await receiverStreamQueue.next as SendPort;

    thisTerminal.onOutput = sender.send;
    thisTerminal.onResize = (w, h, pw, ph) => sender.send([w, h, pw, ph]);

    receiverStreamQueue.rest.listen((data) {
      if (data is String) {
        thisTerminal.write(data);
      } else if (data is int?) {
        isolate.kill(priority: Isolate.immediate);
        setState(() => terminal = null);
      }
    });
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
        color: Colors.black,
        alignment: Alignment.center,
        child: OutlinedButton(
          style: buttonStyle,
          onPressed: !vmRunning
              ? null
              : () => setState(() {
                    terminal = Terminal(maxLines: 10000);
                    initTerminal();
                  }),
          child: const Text('Open shell'),
        ),
      );
    }

    return TerminalView(
      thisTerminal,
      autofocus: true,
      shortcuts: const {},
      hardwareKeyboardOnly: true,
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

  final client = SSHClient(
    await SSHSocket.connect(sshInfo.host, sshInfo.port),
    username: sshInfo.username,
    identities: [rsa.getPrivateKeys()],
    keepAliveInterval: const Duration(seconds: 1),
  );

  final session = await client.shell(
    pty: SSHPtyConfig(width: width, height: height),
  );

  session.done.then((_) => sender.send(session.exitCode));

  receiver.listen((event) {
    if (event is String) {
      session.write(utf8.encoder.convert(event));
    } else if (event case [final w, final h, final pw, final ph]) {
      session.resizeTerminal(w, h, pw, ph);
    }
  });

  StreamGroup.merge([session.stdout, session.stderr])
      .listen((event) => info.sender.send(utf8.decoder.convert(event)));
}
