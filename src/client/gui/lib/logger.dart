import 'dart:io';

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
    printer: PrettyPrinter(
      colors: false,
      lineLength: 20,
      printTime: true,
      methodCount: 0,
      levelEmojis: {
        Level.debug: 'DEBUG',
        Level.error: 'ERROR',
        Level.info: 'INFO',
        Level.warning: 'WARN',
      },
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
