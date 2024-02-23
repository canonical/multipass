import 'dart:io';

import 'package:logger/logger.dart';
import 'package:path_provider/path_provider.dart';

late final Logger logger;

Future<void> setupLogger() async {
  final logFilePath = await getApplicationSupportDirectory();

  logger = Logger(
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
      FileOutput(file: File('${logFilePath.path}/multipass_desktop.log')),
    ]),
  );
}
