import 'package:logger/logger.dart';

final logger = Logger(
  output: MultiOutput([
    ConsoleOutput(),
  ]),
);
