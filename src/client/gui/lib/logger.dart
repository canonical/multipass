import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:logger/logger.dart';
import 'package:path_provider/path_provider.dart';

late final Logger logger;

class NoFilter extends LogFilter {
  @override
  bool shouldLog(LogEvent event) => true;
}

Future<void> setupLogger() async {
  final logFilePath = await getApplicationSupportDirectory();

  logger = Logger(
    filter: NoFilter(),
    printer: MpPrettyPrinter(
      excludePaths: [
        'dart:',
        'package:flutter',
      ],
    ),
    output: MultiOutput([
      ConsoleOutput(),
      FileOutput(file: File('${logFilePath.path}/multipass_gui.log')),
    ]),
  );

  FlutterError.onError = (details) {
    logger.e(
      'Flutter error',
      error: details.exception,
      stackTrace: details.stack,
    );
  };
  PlatformDispatcher.instance.onError = (error, stackTrace) {
    logger.e(
      'Dart error',
      error: error,
      stackTrace: stackTrace,
    );
    return true;
  };
}

class MpPrettyPrinter extends LogPrinter {
  static final _deviceStackTraceRegex = RegExp(r'#[0-9]+\s+(.+) \((\S+)\)');
  static final _webStackTraceRegex = RegExp(r'^((packages|dart-sdk)/\S+/)');
  static final _browserStackTraceRegex =
      RegExp(r'^(?:package:)?(dart:\S+|\S+)');
  final int stackTraceBeginIndex;
  final int? methodCount;
  final int? errorMethodCount;
  final List<String> excludePaths;

  MpPrettyPrinter({
    this.stackTraceBeginIndex = 0,
    this.methodCount = 0,
    this.errorMethodCount = 8,
    this.excludePaths = const [],
  });

  @override
  List<String> log(final LogEvent event) {
    String? stackTraceStr;
    if (event.error != null) {
      if ((errorMethodCount == null || errorMethodCount! > 0)) {
        stackTraceStr = formatStackTrace(
          event.stackTrace ?? StackTrace.current,
          errorMethodCount,
        );
      }
    } else if (methodCount == null || methodCount! > 0) {
      stackTraceStr = formatStackTrace(
        event.stackTrace ?? StackTrace.current,
        methodCount,
      );
    }

    return _formatAndPrint(
      event.level,
      stringifyMessage(event.message),
      event.time.toString(),
      event.error?.toString(),
      stackTraceStr,
    );
  }

  String? formatStackTrace(StackTrace? stackTrace, int? methodCount) {
    final lines = stackTrace.toString().split('\n').where((line) {
      return !_discardDeviceStacktraceLine(line) &&
          !_discardWebStacktraceLine(line) &&
          !_discardBrowserStacktraceLine(line) &&
          line.isNotEmpty;
    }).toList();

    final formatted = <String>[];
    final stackTraceLength =
        methodCount != null ? min(lines.length, methodCount) : lines.length;

    for (var count = 0; count < stackTraceLength; count++) {
      final line = lines[count];
      if (count < stackTraceBeginIndex) continue;
      formatted.add('#$count   ${line.replaceFirst(RegExp(r'#\d+\s+'), '')}');
    }

    return formatted.isEmpty ? null : formatted.join('\n');
  }

  bool _isInExcludePaths(final String segment) {
    return excludePaths.any(segment.startsWith);
  }

  bool _discardDeviceStacktraceLine(String line) {
    final match = _deviceStackTraceRegex.matchAsPrefix(line);
    if (match == null) return false;
    final segment = match.group(2)!;
    if (segment.startsWith('package:logger')) return true;
    return _isInExcludePaths(segment);
  }

  bool _discardWebStacktraceLine(String line) {
    final match = _webStackTraceRegex.matchAsPrefix(line);
    if (match == null) return false;
    final segment = match.group(1)!;
    if (segment.startsWith('packages/logger') ||
        segment.startsWith('dart-sdk/lib')) return true;
    return _isInExcludePaths(segment);
  }

  bool _discardBrowserStacktraceLine(String line) {
    final match = _browserStackTraceRegex.matchAsPrefix(line);
    if (match == null) return false;
    final segment = match.group(1)!;
    if (segment.startsWith('package:logger') || segment.startsWith('dart:')) {
      return true;
    }
    return _isInExcludePaths(segment);
  }

  Object toEncodableFallback(dynamic object) {
    return object.toString();
  }

  String stringifyMessage(dynamic message) {
    final finalMessage = message is Function ? message() : message;
    final encoder = JsonEncoder.withIndent('  ', toEncodableFallback);
    return finalMessage is Map || finalMessage is Iterable
        ? encoder.convert(finalMessage)
        : finalMessage.toString();
  }

  List<String> _formatAndPrint(
    Level level,
    String message,
    String time,
    String? error,
    String? stacktrace,
  ) {
    return [
      '$time ${level.name.toUpperCase()} $message',
      if (error != null) '\t${error.replaceAll('\n', '\n\t')}',
      if (stacktrace != null) '\t${stacktrace.replaceAll('\n', '\n\t')}',
    ];
  }
}
