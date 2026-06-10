import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

/// A minimal valid SVG returned for any `.svg` asset request
const _minimalSvg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1" '
    'preserveAspectRatio="none"/>';

/// An [AssetBundle] that stubs out `.svg` request with [_minimalSvg]
/// and delegates all other requests to [rootBundle].
class FakeSvgAssetBundle extends CachingAssetBundle {
  @override
  Future<ByteData> load(String key) async {
    if (key.endsWith('.svg')) {
      final bytes = utf8.encode(_minimalSvg);
      return ByteData.view(Uint8List.fromList(bytes).buffer);
    }
    return rootBundle.load(key);
  }
}

/// Wraps [child] in a [DefaultAssetBundle] that stubs SVG assets
Widget withFakeSvgAssetBundle(Widget child) =>
    DefaultAssetBundle(bundle: FakeSvgAssetBundle(), child: child);
