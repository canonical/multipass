import 'dart:io';

import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'ffi.dart';
import 'l10n/app_localizations.dart';
import 'providers.dart';
import 'tooltip.dart';
import 'package:flutter/services.dart';

class _CopyErrorIcon extends StatefulWidget {
  final String errorMessage;
  const _CopyErrorIcon({required this.errorMessage});

  @override
  State<_CopyErrorIcon> createState() => _CopyErrorIconState();
}

class _CopyErrorIconState extends State<_CopyErrorIcon> {
  bool _copied = false;

  void _copy() async {
    await Clipboard.setData(ClipboardData(text: widget.errorMessage));
    setState(() => _copied = true);
    Future.delayed(const Duration(seconds: 2), () {
      if (mounted) setState(() => _copied = false);
    });
  }

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    return Tooltip(
      message: _copied ? l10n.daemonCopied : l10n.daemonCopyErrorTooltip,
      child: IconButton(
        icon: const Icon(Icons.copy, size: 20),
        onPressed: _copy,
      ),
    );
  }
}

class _ErrorPanel extends StatelessWidget {
  final String title;
  final String message;

  const _ErrorPanel({required this.title, required this.message});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    return Stack(
      children: [
        Container(
          padding: const EdgeInsets.all(20),
          constraints: const BoxConstraints(maxWidth: 500),
          decoration: const BoxDecoration(
            color: Colors.white,
            boxShadow: [
              BoxShadow(
              color: Colors.black54,
              blurRadius: 10,
              spreadRadius: 5,
              ),
            ],
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.error, color: Colors.red, size: 48),
              const SizedBox(height: 16),
              Text(
                title,
                style: const TextStyle(
                  fontSize: 20,
                  fontWeight: FontWeight.bold,
                  color: Colors.red,
                ),
              ),
              const SizedBox(height: 12),
              SelectableText(
                message,
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 14),
              ),
              const SizedBox(height: 16),
              TextButton(
                onPressed: () => exit(1),
                child: Text(l10n.daemonExitButton),
              ),
            ],
          ),
        ),
        Positioned(
          top: 8,
          right: 8,
          child: _CopyErrorIcon(errorMessage: message),
        ),
      ],
    );
  }
}

class DaemonUnavailable extends ConsumerWidget {
  const DaemonUnavailable({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final available = ref.watch(daemonAvailableProvider);
    final ffiAvailable = ref.watch(ffiAvailableProvider);
    final backendError = ref.watch(daemonBackendErrorProvider);

    if (available && backendError == null) {
      return const SizedBox.shrink();
    }

    Widget content;

    if (!ffiAvailable) {
      // FFI library failed to load, show fatal error
      content = _ErrorPanel(
        title: l10n.daemonFatalError,
        message:
            ffiLoadError?.toString() ?? 'Failed to load libdart_ffi library',
      );
    } else if (backendError != null) {
      // The daemon is reachable, but it returned an error while polling VM
      content =
          _ErrorPanel(title: l10n.daemonBackendError, message: backendError);
    } else {
      // Regular daemon unavailable message
      content = Container(
        padding: const EdgeInsets.all(20),
        decoration: const BoxDecoration(
          color: Colors.white,
          boxShadow: [
            BoxShadow(color: Colors.black54, blurRadius: 10, spreadRadius: 5),
          ],
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            const CircularProgressIndicator(color: Colors.orange),
            const SizedBox(width: 20),
            Text(l10n.daemonWaiting),
          ],
        ),
      );
    }

    final visible = !available || backendError != null;
    return IgnorePointer(
      ignoring: !visible, // Only allow interactions when nothing is shown
      child: AnimatedOpacity(
        opacity: visible ? 1 : 0,
        duration: const Duration(milliseconds: 500),
        child: Scaffold(
          backgroundColor: Colors.black54,
          body: BackdropFilter(
            filter: const ColorFilter.mode(Colors.grey, BlendMode.saturation),
            child: Center(child: content),
          ),
        ),
      ),
    );
  }
}
