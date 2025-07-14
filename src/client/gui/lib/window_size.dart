import 'dart:ui';

import 'package:async/async.dart';
import 'package:basics/basics.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:window_manager/window_manager.dart';
import 'package:window_size/window_size.dart';

import 'logger.dart';

const windowWidthKey = 'windowWidth';
const windowHeightKey = 'windowHeight';

String resolutionString(Size? size) {
  return size != null ? '${size.width}x${size.height}' : '';
}

final saveWindowSizeTimer = RestartableTimer(1.seconds, () async {
  final currentSize = await windowManager.getSize();
  final sharedPreferences = await SharedPreferences.getInstance();
  final screenSize = await getCurrentScreenSize();
  logger.d(
    'Saving window size ${currentSize.s()} for screen size ${screenSize?.s()}',
  );
  final prefix = resolutionString(screenSize);
  sharedPreferences.setDouble('$prefix$windowWidthKey', currentSize.width);
  sharedPreferences.setDouble('$prefix$windowHeightKey', currentSize.height);
});

Future<Size?> getCurrentScreenSize() async {
  try {
    final screen = await getCurrentScreen();
    if (screen == null) throw Exception('Screen instance is null');

    logger.d(
      'Got Screen{frame: ${screen.frame.s()}, scaleFactor: ${screen.scaleFactor}, visibleFrame: ${screen.visibleFrame.s()}}',
    );

    return screen.visibleFrame.size;
  } catch (e) {
    logger.w('Failed to get current screen information: $e');
    return null;
  }
}

Future<Size> deriveWindowSize(SharedPreferences sharedPreferences) async {
  final screenSize = await getCurrentScreenSize();
  final prefix = resolutionString(screenSize);
  final lastWidth = sharedPreferences.getDouble('$prefix$windowWidthKey');
  final lastHeight = sharedPreferences.getDouble('$prefix$windowHeightKey');
  final size = lastWidth != null && lastHeight != null
      ? Size(lastWidth, lastHeight)
      : null;
  logger.d('Got last window size: ${size?.s()}');
  return size ?? computeDefaultWindowSize(screenSize);
}

Size computeDefaultWindowSize(Size? screenSize) {
  const windowSizeFactor = 0.8;
  final (screenWidth, screenHeight) = (screenSize?.width, screenSize?.height);
  final aspectRatioFactor = screenSize?.flipped.aspectRatio;

  final defaultWidth = switch (screenWidth) {
    null || <= 1024 => 750.0,
    >= 1600 => 1400.0,
    _ => screenWidth * windowSizeFactor,
  };

  final defaultHeight = switch (screenHeight) {
    null || <= 576 => 450.0,
    >= 900 => 822.0,
    _ => aspectRatioFactor != null
        ? defaultWidth * aspectRatioFactor
        : screenHeight * windowSizeFactor,
  };

  final size = Size(defaultWidth, defaultHeight);
  logger.d('Computed default window size: ${size.s()}');
  return size;
}

// needed because in release mode Flutter does not emit the actual code for toString for some classes
// instead the returned strings are of type "Instance of '<Type>'"
// this is done to reduce binary size, and it cannot be turned off :face-with-rolling-eyes:
// see https://api.flutter.dev/flutter/dart-ui/keepToString-constant.html for more info
extension on Size {
  String s() {
    return 'Size(${width.toStringAsFixed(1)}, ${height.toStringAsFixed(1)})';
  }
}

extension on Rect {
  String s() {
    return 'Rect.fromLTRB(${left.toStringAsFixed(1)}, ${top.toStringAsFixed(1)}, ${right.toStringAsFixed(1)}, ${bottom.toStringAsFixed(1)})';
  }
}
