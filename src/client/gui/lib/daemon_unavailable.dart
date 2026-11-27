import 'dart:io';

import 'package:material_ui/material_ui.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'colors.dart';
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

class DaemonUnavailable extends ConsumerWidget {
  const DaemonUnavailable({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final available = ref.watch(daemonAvailableProvider);
    final ffiAvailable = ref.watch(ffiAvailableProvider);
    final colors = context.colors;
    final dialogDecoration = BoxDecoration(
      color: colors.surface,
      boxShadow: [
        BoxShadow(color: colors.shadow, blurRadius: 10, spreadRadius: 5),
      ],
    );

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
            decoration: dialogDecoration,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(Icons.error, color: colors.error, size: 48),
                const SizedBox(height: 16),
                Text(
                  l10n.daemonFatalError,
                  style: TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                    color: colors.error,
                  ),
                ),
                const SizedBox(height: 12),
                Theme(
                  data: Theme.of(context).copyWith(
                    textButtonTheme: TextButtonThemeData(
                      style: TextButton.styleFrom(
                        backgroundColor: colors.surface,
                        foregroundColor: colors.textPrimary,
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
                      child: Text(l10n.daemonExitButton),
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
        decoration: dialogDecoration,
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(color: colors.loadingIndicator),
            const SizedBox(width: 20),
            Text(l10n.daemonWaiting),
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
          backgroundColor: colors.scrim,
          body: BackdropFilter(
            filter: const ColorFilter.mode(Colors.grey, BlendMode.saturation),
            child: Center(child: content),
          ),
        ),
      ),
    );
  }
}
