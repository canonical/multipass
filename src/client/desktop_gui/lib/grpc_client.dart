import 'generated/multipass.pbgrpc.dart';

class GrpcClient {
  final RpcClient _client;

  GrpcClient(this._client);

  Future<StartReply?> start(Iterable<String> names) async {
    final replyStream = _client.start(Stream.value(StartRequest(
      instanceNames: InstanceNames(instanceName: names),
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Future<StopReply?> stop(Iterable<String> names) async {
    final replyStream = _client.stop(Stream.value(StopRequest(
      instanceNames: InstanceNames(instanceName: names),
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Future<SuspendReply?> suspend(Iterable<String> names) async {
    final replyStream = _client.suspend(Stream.value(SuspendRequest(
      instanceNames: InstanceNames(instanceName: names),
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Future<RestartReply?> restart(Iterable<String> names) async {
    final replyStream = _client.restart(Stream.value(RestartRequest(
      instanceNames: InstanceNames(instanceName: names),
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Future<DeleteReply?> delete(Iterable<String> names) async {
    final replyStream = _client.delet(Stream.value(DeleteRequest(
      instanceNames: InstanceNames(instanceName: names),
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Future<RecoverReply?> recover(Iterable<String> names) async {
    final replyStream = _client.recover(Stream.value(RecoverRequest(
      instanceNames: InstanceNames(instanceName: names),
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Future<DeleteReply?> purge(Iterable<String> names) async {
    final replyStream = _client.delet(Stream.value(DeleteRequest(
      instanceNames: InstanceNames(instanceName: names),
      purge: true,
    )));
    await for (final reply in replyStream) {
      return reply;
    }
    return null;
  }

  Stream<InfoReply> infoStream() async* {
    await for (final _ in Stream.periodic(const Duration(seconds: 1))) {
      try {
        yield await _client.info(Stream.value(InfoRequest())).last;
      } catch (e, stackTrace) {
        yield* Stream.error(e, stackTrace);
      }
    }
  }
}
