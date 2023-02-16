///
//  Generated code. Do not modify.
//  source: multipass.proto
//
// @dart = 2.12
// ignore_for_file: annotate_overrides,camel_case_types,constant_identifier_names,directives_ordering,library_prefixes,non_constant_identifier_names,prefer_final_fields,return_of_invalid_type,unnecessary_const,unnecessary_import,unnecessary_this,unused_import,unused_shown_name

import 'dart:async' as $async;

import 'dart:core' as $core;

import 'package:grpc/service_api.dart' as $grpc;
import 'multipass.pb.dart' as $0;
export 'multipass.pb.dart';

class RpcClient extends $grpc.Client {
  static final _$create = $grpc.ClientMethod<$0.LaunchRequest, $0.LaunchReply>(
      '/multipass.Rpc/create',
      ($0.LaunchRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.LaunchReply.fromBuffer(value));
  static final _$launch = $grpc.ClientMethod<$0.LaunchRequest, $0.LaunchReply>(
      '/multipass.Rpc/launch',
      ($0.LaunchRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.LaunchReply.fromBuffer(value));
  static final _$purge = $grpc.ClientMethod<$0.PurgeRequest, $0.PurgeReply>(
      '/multipass.Rpc/purge',
      ($0.PurgeRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.PurgeReply.fromBuffer(value));
  static final _$find = $grpc.ClientMethod<$0.FindRequest, $0.FindReply>(
      '/multipass.Rpc/find',
      ($0.FindRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.FindReply.fromBuffer(value));
  static final _$info = $grpc.ClientMethod<$0.InfoRequest, $0.InfoReply>(
      '/multipass.Rpc/info',
      ($0.InfoRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.InfoReply.fromBuffer(value));
  static final _$list = $grpc.ClientMethod<$0.ListRequest, $0.ListReply>(
      '/multipass.Rpc/list',
      ($0.ListRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.ListReply.fromBuffer(value));
  static final _$networks =
      $grpc.ClientMethod<$0.NetworksRequest, $0.NetworksReply>(
          '/multipass.Rpc/networks',
          ($0.NetworksRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) => $0.NetworksReply.fromBuffer(value));
  static final _$mount = $grpc.ClientMethod<$0.MountRequest, $0.MountReply>(
      '/multipass.Rpc/mount',
      ($0.MountRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.MountReply.fromBuffer(value));
  static final _$ping = $grpc.ClientMethod<$0.PingRequest, $0.PingReply>(
      '/multipass.Rpc/ping',
      ($0.PingRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.PingReply.fromBuffer(value));
  static final _$recover =
      $grpc.ClientMethod<$0.RecoverRequest, $0.RecoverReply>(
          '/multipass.Rpc/recover',
          ($0.RecoverRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) => $0.RecoverReply.fromBuffer(value));
  static final _$ssh_info =
      $grpc.ClientMethod<$0.SSHInfoRequest, $0.SSHInfoReply>(
          '/multipass.Rpc/ssh_info',
          ($0.SSHInfoRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) => $0.SSHInfoReply.fromBuffer(value));
  static final _$start = $grpc.ClientMethod<$0.StartRequest, $0.StartReply>(
      '/multipass.Rpc/start',
      ($0.StartRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.StartReply.fromBuffer(value));
  static final _$stop = $grpc.ClientMethod<$0.StopRequest, $0.StopReply>(
      '/multipass.Rpc/stop',
      ($0.StopRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.StopReply.fromBuffer(value));
  static final _$suspend =
      $grpc.ClientMethod<$0.SuspendRequest, $0.SuspendReply>(
          '/multipass.Rpc/suspend',
          ($0.SuspendRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) => $0.SuspendReply.fromBuffer(value));
  static final _$restart =
      $grpc.ClientMethod<$0.RestartRequest, $0.RestartReply>(
          '/multipass.Rpc/restart',
          ($0.RestartRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) => $0.RestartReply.fromBuffer(value));
  static final _$delet = $grpc.ClientMethod<$0.DeleteRequest, $0.DeleteReply>(
      '/multipass.Rpc/delet',
      ($0.DeleteRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.DeleteReply.fromBuffer(value));
  static final _$umount = $grpc.ClientMethod<$0.UmountRequest, $0.UmountReply>(
      '/multipass.Rpc/umount',
      ($0.UmountRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.UmountReply.fromBuffer(value));
  static final _$version =
      $grpc.ClientMethod<$0.VersionRequest, $0.VersionReply>(
          '/multipass.Rpc/version',
          ($0.VersionRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) => $0.VersionReply.fromBuffer(value));
  static final _$get = $grpc.ClientMethod<$0.GetRequest, $0.GetReply>(
      '/multipass.Rpc/get',
      ($0.GetRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.GetReply.fromBuffer(value));
  static final _$set = $grpc.ClientMethod<$0.SetRequest, $0.SetReply>(
      '/multipass.Rpc/set',
      ($0.SetRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.SetReply.fromBuffer(value));
  static final _$keys = $grpc.ClientMethod<$0.KeysRequest, $0.KeysReply>(
      '/multipass.Rpc/keys',
      ($0.KeysRequest value) => value.writeToBuffer(),
      ($core.List<$core.int> value) => $0.KeysReply.fromBuffer(value));
  static final _$authenticate =
      $grpc.ClientMethod<$0.AuthenticateRequest, $0.AuthenticateReply>(
          '/multipass.Rpc/authenticate',
          ($0.AuthenticateRequest value) => value.writeToBuffer(),
          ($core.List<$core.int> value) =>
              $0.AuthenticateReply.fromBuffer(value));

  RpcClient($grpc.ClientChannel channel,
      {$grpc.CallOptions? options,
      $core.Iterable<$grpc.ClientInterceptor>? interceptors})
      : super(channel, options: options, interceptors: interceptors);

  $grpc.ResponseStream<$0.LaunchReply> create(
      $async.Stream<$0.LaunchRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$create, request, options: options);
  }

  $grpc.ResponseStream<$0.LaunchReply> launch(
      $async.Stream<$0.LaunchRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$launch, request, options: options);
  }

  $grpc.ResponseStream<$0.PurgeReply> purge(
      $async.Stream<$0.PurgeRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$purge, request, options: options);
  }

  $grpc.ResponseStream<$0.FindReply> find($async.Stream<$0.FindRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$find, request, options: options);
  }

  $grpc.ResponseStream<$0.InfoReply> info($async.Stream<$0.InfoRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$info, request, options: options);
  }

  $grpc.ResponseStream<$0.ListReply> list($async.Stream<$0.ListRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$list, request, options: options);
  }

  $grpc.ResponseStream<$0.NetworksReply> networks(
      $async.Stream<$0.NetworksRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$networks, request, options: options);
  }

  $grpc.ResponseStream<$0.MountReply> mount(
      $async.Stream<$0.MountRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$mount, request, options: options);
  }

  $grpc.ResponseFuture<$0.PingReply> ping($0.PingRequest request,
      {$grpc.CallOptions? options}) {
    return $createUnaryCall(_$ping, request, options: options);
  }

  $grpc.ResponseStream<$0.RecoverReply> recover(
      $async.Stream<$0.RecoverRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$recover, request, options: options);
  }

  $grpc.ResponseStream<$0.SSHInfoReply> ssh_info(
      $async.Stream<$0.SSHInfoRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$ssh_info, request, options: options);
  }

  $grpc.ResponseStream<$0.StartReply> start(
      $async.Stream<$0.StartRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$start, request, options: options);
  }

  $grpc.ResponseStream<$0.StopReply> stop($async.Stream<$0.StopRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$stop, request, options: options);
  }

  $grpc.ResponseStream<$0.SuspendReply> suspend(
      $async.Stream<$0.SuspendRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$suspend, request, options: options);
  }

  $grpc.ResponseStream<$0.RestartReply> restart(
      $async.Stream<$0.RestartRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$restart, request, options: options);
  }

  $grpc.ResponseStream<$0.DeleteReply> delet(
      $async.Stream<$0.DeleteRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$delet, request, options: options);
  }

  $grpc.ResponseStream<$0.UmountReply> umount(
      $async.Stream<$0.UmountRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$umount, request, options: options);
  }

  $grpc.ResponseStream<$0.VersionReply> version(
      $async.Stream<$0.VersionRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$version, request, options: options);
  }

  $grpc.ResponseStream<$0.GetReply> get($async.Stream<$0.GetRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$get, request, options: options);
  }

  $grpc.ResponseStream<$0.SetReply> set($async.Stream<$0.SetRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$set, request, options: options);
  }

  $grpc.ResponseStream<$0.KeysReply> keys($async.Stream<$0.KeysRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$keys, request, options: options);
  }

  $grpc.ResponseStream<$0.AuthenticateReply> authenticate(
      $async.Stream<$0.AuthenticateRequest> request,
      {$grpc.CallOptions? options}) {
    return $createStreamingCall(_$authenticate, request, options: options);
  }
}

abstract class RpcServiceBase extends $grpc.Service {
  $core.String get $name => 'multipass.Rpc';

  RpcServiceBase() {
    $addMethod($grpc.ServiceMethod<$0.LaunchRequest, $0.LaunchReply>(
        'create',
        create,
        true,
        true,
        ($core.List<$core.int> value) => $0.LaunchRequest.fromBuffer(value),
        ($0.LaunchReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.LaunchRequest, $0.LaunchReply>(
        'launch',
        launch,
        true,
        true,
        ($core.List<$core.int> value) => $0.LaunchRequest.fromBuffer(value),
        ($0.LaunchReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.PurgeRequest, $0.PurgeReply>(
        'purge',
        purge,
        true,
        true,
        ($core.List<$core.int> value) => $0.PurgeRequest.fromBuffer(value),
        ($0.PurgeReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.FindRequest, $0.FindReply>(
        'find',
        find,
        true,
        true,
        ($core.List<$core.int> value) => $0.FindRequest.fromBuffer(value),
        ($0.FindReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.InfoRequest, $0.InfoReply>(
        'info',
        info,
        true,
        true,
        ($core.List<$core.int> value) => $0.InfoRequest.fromBuffer(value),
        ($0.InfoReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ListRequest, $0.ListReply>(
        'list',
        list,
        true,
        true,
        ($core.List<$core.int> value) => $0.ListRequest.fromBuffer(value),
        ($0.ListReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.NetworksRequest, $0.NetworksReply>(
        'networks',
        networks,
        true,
        true,
        ($core.List<$core.int> value) => $0.NetworksRequest.fromBuffer(value),
        ($0.NetworksReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.MountRequest, $0.MountReply>(
        'mount',
        mount,
        true,
        true,
        ($core.List<$core.int> value) => $0.MountRequest.fromBuffer(value),
        ($0.MountReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.PingRequest, $0.PingReply>(
        'ping',
        ping_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.PingRequest.fromBuffer(value),
        ($0.PingReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.RecoverRequest, $0.RecoverReply>(
        'recover',
        recover,
        true,
        true,
        ($core.List<$core.int> value) => $0.RecoverRequest.fromBuffer(value),
        ($0.RecoverReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.SSHInfoRequest, $0.SSHInfoReply>(
        'ssh_info',
        ssh_info,
        true,
        true,
        ($core.List<$core.int> value) => $0.SSHInfoRequest.fromBuffer(value),
        ($0.SSHInfoReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.StartRequest, $0.StartReply>(
        'start',
        start,
        true,
        true,
        ($core.List<$core.int> value) => $0.StartRequest.fromBuffer(value),
        ($0.StartReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.StopRequest, $0.StopReply>(
        'stop',
        stop,
        true,
        true,
        ($core.List<$core.int> value) => $0.StopRequest.fromBuffer(value),
        ($0.StopReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.SuspendRequest, $0.SuspendReply>(
        'suspend',
        suspend,
        true,
        true,
        ($core.List<$core.int> value) => $0.SuspendRequest.fromBuffer(value),
        ($0.SuspendReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.RestartRequest, $0.RestartReply>(
        'restart',
        restart,
        true,
        true,
        ($core.List<$core.int> value) => $0.RestartRequest.fromBuffer(value),
        ($0.RestartReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.DeleteRequest, $0.DeleteReply>(
        'delet',
        delet,
        true,
        true,
        ($core.List<$core.int> value) => $0.DeleteRequest.fromBuffer(value),
        ($0.DeleteReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.UmountRequest, $0.UmountReply>(
        'umount',
        umount,
        true,
        true,
        ($core.List<$core.int> value) => $0.UmountRequest.fromBuffer(value),
        ($0.UmountReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.VersionRequest, $0.VersionReply>(
        'version',
        version,
        true,
        true,
        ($core.List<$core.int> value) => $0.VersionRequest.fromBuffer(value),
        ($0.VersionReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.GetRequest, $0.GetReply>(
        'get',
        get,
        true,
        true,
        ($core.List<$core.int> value) => $0.GetRequest.fromBuffer(value),
        ($0.GetReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.SetRequest, $0.SetReply>(
        'set',
        set,
        true,
        true,
        ($core.List<$core.int> value) => $0.SetRequest.fromBuffer(value),
        ($0.SetReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.KeysRequest, $0.KeysReply>(
        'keys',
        keys,
        true,
        true,
        ($core.List<$core.int> value) => $0.KeysRequest.fromBuffer(value),
        ($0.KeysReply value) => value.writeToBuffer()));
    $addMethod(
        $grpc.ServiceMethod<$0.AuthenticateRequest, $0.AuthenticateReply>(
            'authenticate',
            authenticate,
            true,
            true,
            ($core.List<$core.int> value) =>
                $0.AuthenticateRequest.fromBuffer(value),
            ($0.AuthenticateReply value) => value.writeToBuffer()));
  }

  $async.Future<$0.PingReply> ping_Pre(
      $grpc.ServiceCall call, $async.Future<$0.PingRequest> request) async {
    return ping(call, await request);
  }

  $async.Stream<$0.LaunchReply> create(
      $grpc.ServiceCall call, $async.Stream<$0.LaunchRequest> request);
  $async.Stream<$0.LaunchReply> launch(
      $grpc.ServiceCall call, $async.Stream<$0.LaunchRequest> request);
  $async.Stream<$0.PurgeReply> purge(
      $grpc.ServiceCall call, $async.Stream<$0.PurgeRequest> request);
  $async.Stream<$0.FindReply> find(
      $grpc.ServiceCall call, $async.Stream<$0.FindRequest> request);
  $async.Stream<$0.InfoReply> info(
      $grpc.ServiceCall call, $async.Stream<$0.InfoRequest> request);
  $async.Stream<$0.ListReply> list(
      $grpc.ServiceCall call, $async.Stream<$0.ListRequest> request);
  $async.Stream<$0.NetworksReply> networks(
      $grpc.ServiceCall call, $async.Stream<$0.NetworksRequest> request);
  $async.Stream<$0.MountReply> mount(
      $grpc.ServiceCall call, $async.Stream<$0.MountRequest> request);
  $async.Future<$0.PingReply> ping(
      $grpc.ServiceCall call, $0.PingRequest request);
  $async.Stream<$0.RecoverReply> recover(
      $grpc.ServiceCall call, $async.Stream<$0.RecoverRequest> request);
  $async.Stream<$0.SSHInfoReply> ssh_info(
      $grpc.ServiceCall call, $async.Stream<$0.SSHInfoRequest> request);
  $async.Stream<$0.StartReply> start(
      $grpc.ServiceCall call, $async.Stream<$0.StartRequest> request);
  $async.Stream<$0.StopReply> stop(
      $grpc.ServiceCall call, $async.Stream<$0.StopRequest> request);
  $async.Stream<$0.SuspendReply> suspend(
      $grpc.ServiceCall call, $async.Stream<$0.SuspendRequest> request);
  $async.Stream<$0.RestartReply> restart(
      $grpc.ServiceCall call, $async.Stream<$0.RestartRequest> request);
  $async.Stream<$0.DeleteReply> delet(
      $grpc.ServiceCall call, $async.Stream<$0.DeleteRequest> request);
  $async.Stream<$0.UmountReply> umount(
      $grpc.ServiceCall call, $async.Stream<$0.UmountRequest> request);
  $async.Stream<$0.VersionReply> version(
      $grpc.ServiceCall call, $async.Stream<$0.VersionRequest> request);
  $async.Stream<$0.GetReply> get(
      $grpc.ServiceCall call, $async.Stream<$0.GetRequest> request);
  $async.Stream<$0.SetReply> set(
      $grpc.ServiceCall call, $async.Stream<$0.SetRequest> request);
  $async.Stream<$0.KeysReply> keys(
      $grpc.ServiceCall call, $async.Stream<$0.KeysRequest> request);
  $async.Stream<$0.AuthenticateReply> authenticate(
      $grpc.ServiceCall call, $async.Stream<$0.AuthenticateRequest> request);
}
