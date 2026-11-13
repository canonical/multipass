import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/providers.dart';

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

/// Fake gRPC client
///
/// All methods return empty/default replies. Override individual methods in a
/// subclass when a test needs specific behaviour.
class FakeGrpcClient extends GrpcClient {
  FakeGrpcClient() : super(RpcClient(ClientChannel('localhost')));

  final List<({String method, Iterable<String> names})> calls = [];

  @override
  Future<StartReply?> start(Iterable<String> names) async {
    calls.add((method: 'start', names: names));
    return StartReply();
  }

  @override
  Future<StopReply?> stop(Iterable<String> names) async {
    calls.add((method: 'stop', names: names));
    return StopReply();
  }

  @override
  Future<SuspendReply?> suspend(Iterable<String> names) async {
    calls.add((method: 'suspend', names: names));
    return SuspendReply();
  }

  @override
  Future<DeleteReply?> delete(Iterable<String> names) async {
    calls.add((method: 'delete', names: names));
    return DeleteReply();
  }

  @override
  Future<DeleteReply?> purge(Iterable<String> names) async {
    calls.add((method: 'purge', names: names));
    return DeleteReply();
  }
}
