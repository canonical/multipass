import 'dart:io';

import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'ffi.dart';
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
    return Tooltip(
      message: _copied ? 'Copied!' : 'Copy error message',
      child: IconButton(
        icon: const Icon(Icons.copy, size: 20),
        onPressed: _copy,
      ),
    );
  }
}

class DaemonUnavailable extends ConsumerWidget {
  const DaemonUnavailable({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final available = ref.watch(daemonAvailableProvider);
    final ffiAvailable = ref.watch(ffiAvailableProvider);

    if (available) {
      return const SizedBox.shrink();
    }

    Widget content;

    if (!ffiAvailable) {
      // FFI library failed to load - show fatal error
      final errorMessage =
          ffiLoadError?.toString() ?? 'Failed to load libdart_ffi library';
      content = Stack(
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
                const Text(
                  'Fatal Error',
                  style: TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                    color: Colors.red,
                  ),
                ),
                const SizedBox(height: 12),
                Theme(
                  data: Theme.of(context).copyWith(
                    textButtonTheme: TextButtonThemeData(
                      style: TextButton.styleFrom(
                        backgroundColor: Colors.white,
                        foregroundColor: Colors.black,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(4),
                        ),
                      ),
                    ),
                  ),
                  child: SelectableText(
                    errorMessage,
                    textAlign: TextAlign.center,
                    style: const TextStyle(fontSize: 14),
                  ),
                ),
                const SizedBox(height: 16),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    TextButton(
                      onPressed: () => exit(1),
                      child: const Text('Exit Application'),
                    ),
                  ],
                ),
              ],
            ),
          ),
          Positioned(
            top: 8,
            right: 8,
            child: _CopyErrorIcon(errorMessage: errorMessage),
          ),
        ],
      );
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
        child: const Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(color: Colors.orange),
            SizedBox(width: 20),
            Text('Waiting for daemon...'),
          ],
        ),
      );
    }

    return IgnorePointer(
      ignoring: available, // Only allow interactions when daemon is available
      child: AnimatedOpacity(
        opacity: available ? 0 : 1,
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
