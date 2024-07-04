import 'dart:convert';
import 'dart:isolate';

import 'package:async/async.dart';
import 'package:dartssh2/dartssh2.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:synchronized/synchronized.dart';
import 'package:xterm/xterm.dart';

import '../logger.dart';
import '../notifications.dart';
import '../platform/platform.dart';
import '../providers.dart';

final runningShellsProvider =
    StateProvider.autoDispose.family<int, String>((_, __) {
  return 0;
});

class ShellId {
  final int id;
  final bool autostart;

  ShellId(this.id, {this.autostart = true});

  @override
  String toString() => 'ShellId{$id}';
}

typedef TerminalIdentifier = ({String vmName, ShellId shellId});

class TerminalNotifier
    extends AutoDisposeFamilyNotifier<Terminal?, TerminalIdentifier> {
  final lock = Lock();
  Isolate? isolate;
  late final vmStatusProvider = vmInfoProvider(arg.vmName).select((info) {
    return info.instanceStatus.status == Status.RUNNING;
  });

  @override
  Terminal? build(TerminalIdentifier arg) {
    ref.onDispose(_dispose);
    if (arg.shellId.autostart) {
      lock.synchronized(_initShell).then((value) => state = value);
    }
    ref.listen(vmStatusProvider, (previous, next) {
      if ((previous ?? false) && !next) stop();
    });
    return null;
  }

  Future<Terminal?> _initShell() async {
    final currentState = stateOrNull;
    if (currentState != null) return currentState;

    final running = ref.read(vmStatusProvider);
    if (!running) return null;

    final grpcClient = ref.read(grpcClientProvider);
    final sshInfo = await grpcClient.sshInfo(arg.vmName).onError((err, stack) {
      ref
          .notifyError((error) => 'Failed to get SSH information: $err')
          .call(err, stack);
      return null;
    });
    if (sshInfo == null) return null;

    final terminal = Terminal(maxLines: 10000);
    final receiver = ReceivePort();
    final errorReceiver = ReceivePort();
    final exitReceiver = ReceivePort();

    errorReceiver.listen((es) {
      final (Object? error, String? stack) = (es[0], es[1]);
      logger.e(
        'Error from $arg ssh isolate',
        error: error,
        stackTrace: stack != null ? StackTrace.fromString(stack) : null,
      );
    });

    exitReceiver.listen((_) {
      logger.d('Exited $arg ssh isolate');
      receiver.close();
      errorReceiver.close();
      exitReceiver.close();
      stop();
    });

    receiver.listen((event) {
      switch (event) {
        case final SendPort sender:
          terminal.onOutput = sender.send;
          terminal.onResize = (w, h, pw, ph) => sender.send([w, h, pw, ph]);
        case final String data:
          terminal.write(data);
        case null:
          stop();
      }
    });

    isolate = await Isolate.spawn(
      sshIsolate,
      SshShellInfo(
        sender: receiver.sendPort,
        width: terminal.viewWidth,
        height: terminal.viewHeight,
        sshInfo: sshInfo,
      ),
      onError: errorReceiver.sendPort,
      onExit: exitReceiver.sendPort,
      errorsAreFatal: true,
    );

    ref.read(runningShellsProvider(arg.vmName).notifier).update((state) {
      return state + 1;
    });
    return terminal;
  }

  Future<void> start() async {
    state = await lock.synchronized(_initShell);
  }

  void stop() {
    _dispose();
    state = null;
  }

  void _dispose() {
    isolate?.kill(priority: Isolate.immediate);
    if (isolate != null) {
      ref
          .read(runningShellsProvider(arg.vmName).notifier)
          .update((state) => state - 1);
    }
    isolate = null;
  }
}

final terminalProvider = NotifierProvider.autoDispose
    .family<TerminalNotifier, Terminal?, TerminalIdentifier>(
        TerminalNotifier.new);

class VmTerminal extends ConsumerStatefulWidget {
  final String name;
  final ShellId id;
  final bool isCurrent;

  const VmTerminal(
    this.name,
    this.id, {
    super.key,
    this.isCurrent = false,
  });

  @override
  ConsumerState<VmTerminal> createState() => _VmTerminalState();
}

class _VmTerminalState extends ConsumerState<VmTerminal> {
  final scrollController = ScrollController();
  final focusNode = FocusNode();

  @override
  void initState() {
    super.initState();
    focusNode.requestFocus();
  }

  @override
  void dispose() {
    scrollController.dispose();
    focusNode.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(VmTerminal oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.isCurrent) focusNode.requestFocus();
  }

  @override
  Widget build(BuildContext context) {
    final terminalIdentifier = (vmName: widget.name, shellId: widget.id);
    final terminal = ref.watch(terminalProvider(terminalIdentifier));
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

    if (terminal == null) {
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
              onPressed: vmRunning
                  ? () => ref
                      .read(terminalProvider(terminalIdentifier).notifier)
                      .start()
                  : null,
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
          terminal,
          scrollController: scrollController,
          focusNode: focusNode,
          shortcuts: mpPlatform.terminalShortcuts,
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
