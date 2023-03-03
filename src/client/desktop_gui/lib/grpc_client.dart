import 'generated/multipass.pbgrpc.dart';

class GrpcClient {
  final RpcClient _client;

  GrpcClient(this._client);

  Future<StartReply> start(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .start(Stream.value(StartRequest(instanceNames: instanceNames)))
        .single;
  }

  Future<StopReply> stop(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .stop(Stream.value(StopRequest(instanceNames: instanceNames)))
        .single;
  }

  Future<SuspendReply> suspend(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .suspend(Stream.value(SuspendRequest(instanceNames: instanceNames)))
        .single;
  }

  Future<RestartReply> restart(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .restart(Stream.value(RestartRequest(instanceNames: instanceNames)))
        .single;
  }

  Future<DeleteReply> delete(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .delet(Stream.value(DeleteRequest(instanceNames: instanceNames)))
        .single;
  }

  Future<RecoverReply> recover(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .recover(Stream.value(RecoverRequest(instanceNames: instanceNames)))
        .single;
  }

  Future<DeleteReply> purge(Iterable<String> names) {
    final instanceNames = InstanceNames(instanceName: names);
    return _client
        .delet(Stream.value(DeleteRequest(
          instanceNames: instanceNames,
          purge: true,
        )))
        .single;
  }

  Stream<InfoReply> infoStream() async* {
    await for (final _ in Stream.periodic(const Duration(seconds: 1))) {
      try {
        yield await _client.info(Stream.value(InfoRequest())).single;
      } catch (e, stackTrace) {
        yield* Stream.error(e, stackTrace);
      }
    }
  }
}
