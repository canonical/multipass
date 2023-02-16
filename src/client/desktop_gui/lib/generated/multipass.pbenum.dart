///
//  Generated code. Do not modify.
//  source: multipass.proto
//
// @dart = 2.12
// ignore_for_file: annotate_overrides,camel_case_types,constant_identifier_names,directives_ordering,library_prefixes,non_constant_identifier_names,prefer_final_fields,return_of_invalid_type,unnecessary_const,unnecessary_import,unnecessary_this,unused_import,unused_shown_name

// ignore_for_file: UNDEFINED_SHOWN_NAME
import 'dart:core' as $core;
import 'package:protobuf/protobuf.dart' as $pb;

class LaunchRequest_NetworkOptions_Mode extends $pb.ProtobufEnum {
  static const LaunchRequest_NetworkOptions_Mode AUTO = LaunchRequest_NetworkOptions_Mode._(0, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'AUTO');
  static const LaunchRequest_NetworkOptions_Mode MANUAL = LaunchRequest_NetworkOptions_Mode._(1, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'MANUAL');

  static const $core.List<LaunchRequest_NetworkOptions_Mode> values = <LaunchRequest_NetworkOptions_Mode> [
    AUTO,
    MANUAL,
  ];

  static final $core.Map<$core.int, LaunchRequest_NetworkOptions_Mode> _byValue = $pb.ProtobufEnum.initByValue(values);
  static LaunchRequest_NetworkOptions_Mode? valueOf($core.int value) => _byValue[value];

  const LaunchRequest_NetworkOptions_Mode._($core.int v, $core.String n) : super(v, n);
}

class LaunchError_ErrorCodes extends $pb.ProtobufEnum {
  static const LaunchError_ErrorCodes OK = LaunchError_ErrorCodes._(0, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'OK');
  static const LaunchError_ErrorCodes INSTANCE_EXISTS = LaunchError_ErrorCodes._(1, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INSTANCE_EXISTS');
  static const LaunchError_ErrorCodes INVALID_MEM_SIZE = LaunchError_ErrorCodes._(2, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INVALID_MEM_SIZE');
  static const LaunchError_ErrorCodes INVALID_DISK_SIZE = LaunchError_ErrorCodes._(3, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INVALID_DISK_SIZE');
  static const LaunchError_ErrorCodes INVALID_HOSTNAME = LaunchError_ErrorCodes._(4, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INVALID_HOSTNAME');
  static const LaunchError_ErrorCodes INVALID_NETWORK = LaunchError_ErrorCodes._(5, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INVALID_NETWORK');

  static const $core.List<LaunchError_ErrorCodes> values = <LaunchError_ErrorCodes> [
    OK,
    INSTANCE_EXISTS,
    INVALID_MEM_SIZE,
    INVALID_DISK_SIZE,
    INVALID_HOSTNAME,
    INVALID_NETWORK,
  ];

  static final $core.Map<$core.int, LaunchError_ErrorCodes> _byValue = $pb.ProtobufEnum.initByValue(values);
  static LaunchError_ErrorCodes? valueOf($core.int value) => _byValue[value];

  const LaunchError_ErrorCodes._($core.int v, $core.String n) : super(v, n);
}

class LaunchProgress_ProgressTypes extends $pb.ProtobufEnum {
  static const LaunchProgress_ProgressTypes IMAGE = LaunchProgress_ProgressTypes._(0, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'IMAGE');
  static const LaunchProgress_ProgressTypes KERNEL = LaunchProgress_ProgressTypes._(1, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'KERNEL');
  static const LaunchProgress_ProgressTypes INITRD = LaunchProgress_ProgressTypes._(2, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INITRD');
  static const LaunchProgress_ProgressTypes EXTRACT = LaunchProgress_ProgressTypes._(3, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'EXTRACT');
  static const LaunchProgress_ProgressTypes VERIFY = LaunchProgress_ProgressTypes._(4, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'VERIFY');
  static const LaunchProgress_ProgressTypes WAITING = LaunchProgress_ProgressTypes._(5, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'WAITING');

  static const $core.List<LaunchProgress_ProgressTypes> values = <LaunchProgress_ProgressTypes> [
    IMAGE,
    KERNEL,
    INITRD,
    EXTRACT,
    VERIFY,
    WAITING,
  ];

  static final $core.Map<$core.int, LaunchProgress_ProgressTypes> _byValue = $pb.ProtobufEnum.initByValue(values);
  static LaunchProgress_ProgressTypes? valueOf($core.int value) => _byValue[value];

  const LaunchProgress_ProgressTypes._($core.int v, $core.String n) : super(v, n);
}

class InstanceStatus_Status extends $pb.ProtobufEnum {
  static const InstanceStatus_Status UNKNOWN = InstanceStatus_Status._(0, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'UNKNOWN');
  static const InstanceStatus_Status RUNNING = InstanceStatus_Status._(1, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'RUNNING');
  static const InstanceStatus_Status STARTING = InstanceStatus_Status._(2, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'STARTING');
  static const InstanceStatus_Status RESTARTING = InstanceStatus_Status._(3, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'RESTARTING');
  static const InstanceStatus_Status STOPPED = InstanceStatus_Status._(4, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'STOPPED');
  static const InstanceStatus_Status DELETED = InstanceStatus_Status._(5, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'DELETED');
  static const InstanceStatus_Status DELAYED_SHUTDOWN = InstanceStatus_Status._(6, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'DELAYED_SHUTDOWN');
  static const InstanceStatus_Status SUSPENDING = InstanceStatus_Status._(7, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'SUSPENDING');
  static const InstanceStatus_Status SUSPENDED = InstanceStatus_Status._(8, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'SUSPENDED');

  static const $core.List<InstanceStatus_Status> values = <InstanceStatus_Status> [
    UNKNOWN,
    RUNNING,
    STARTING,
    RESTARTING,
    STOPPED,
    DELETED,
    DELAYED_SHUTDOWN,
    SUSPENDING,
    SUSPENDED,
  ];

  static final $core.Map<$core.int, InstanceStatus_Status> _byValue = $pb.ProtobufEnum.initByValue(values);
  static InstanceStatus_Status? valueOf($core.int value) => _byValue[value];

  const InstanceStatus_Status._($core.int v, $core.String n) : super(v, n);
}

class MountRequest_MountType extends $pb.ProtobufEnum {
  static const MountRequest_MountType CLASSIC = MountRequest_MountType._(0, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'CLASSIC');
  static const MountRequest_MountType NATIVE = MountRequest_MountType._(1, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'NATIVE');

  static const $core.List<MountRequest_MountType> values = <MountRequest_MountType> [
    CLASSIC,
    NATIVE,
  ];

  static final $core.Map<$core.int, MountRequest_MountType> _byValue = $pb.ProtobufEnum.initByValue(values);
  static MountRequest_MountType? valueOf($core.int value) => _byValue[value];

  const MountRequest_MountType._($core.int v, $core.String n) : super(v, n);
}

class StartError_ErrorCode extends $pb.ProtobufEnum {
  static const StartError_ErrorCode OK = StartError_ErrorCode._(0, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'OK');
  static const StartError_ErrorCode DOES_NOT_EXIST = StartError_ErrorCode._(1, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'DOES_NOT_EXIST');
  static const StartError_ErrorCode INSTANCE_DELETED = StartError_ErrorCode._(2, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'INSTANCE_DELETED');
  static const StartError_ErrorCode OTHER = StartError_ErrorCode._(3, const $core.bool.fromEnvironment('protobuf.omit_enum_names') ? '' : 'OTHER');

  static const $core.List<StartError_ErrorCode> values = <StartError_ErrorCode> [
    OK,
    DOES_NOT_EXIST,
    INSTANCE_DELETED,
    OTHER,
  ];

  static final $core.Map<$core.int, StartError_ErrorCode> _byValue = $pb.ProtobufEnum.initByValue(values);
  static StartError_ErrorCode? valueOf($core.int value) => _byValue[value];

  const StartError_ErrorCode._($core.int v, $core.String n) : super(v, n);
}

