///
//  Generated code. Do not modify.
//  source: multipass.proto
//
// @dart = 2.12
// ignore_for_file: annotate_overrides,camel_case_types,constant_identifier_names,directives_ordering,library_prefixes,non_constant_identifier_names,prefer_final_fields,return_of_invalid_type,unnecessary_const,unnecessary_import,unnecessary_this,unused_import,unused_shown_name

import 'dart:core' as $core;

import 'package:protobuf/protobuf.dart' as $pb;

import 'multipass.pbenum.dart';

export 'multipass.pbenum.dart';

class LaunchRequest_NetworkOptions extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'LaunchRequest.NetworkOptions', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'id')
    ..e<LaunchRequest_NetworkOptions_Mode>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'mode', $pb.PbFieldType.OE, defaultOrMaker: LaunchRequest_NetworkOptions_Mode.AUTO, valueOf: LaunchRequest_NetworkOptions_Mode.valueOf, enumValues: LaunchRequest_NetworkOptions_Mode.values)
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'macAddress')
    ..hasRequiredFields = false
  ;

  LaunchRequest_NetworkOptions._() : super();
  factory LaunchRequest_NetworkOptions({
    $core.String? id,
    LaunchRequest_NetworkOptions_Mode? mode,
    $core.String? macAddress,
  }) {
    final _result = create();
    if (id != null) {
      _result.id = id;
    }
    if (mode != null) {
      _result.mode = mode;
    }
    if (macAddress != null) {
      _result.macAddress = macAddress;
    }
    return _result;
  }
  factory LaunchRequest_NetworkOptions.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory LaunchRequest_NetworkOptions.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  LaunchRequest_NetworkOptions clone() => LaunchRequest_NetworkOptions()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  LaunchRequest_NetworkOptions copyWith(void Function(LaunchRequest_NetworkOptions) updates) => super.copyWith((message) => updates(message as LaunchRequest_NetworkOptions)) as LaunchRequest_NetworkOptions; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static LaunchRequest_NetworkOptions create() => LaunchRequest_NetworkOptions._();
  LaunchRequest_NetworkOptions createEmptyInstance() => create();
  static $pb.PbList<LaunchRequest_NetworkOptions> createRepeated() => $pb.PbList<LaunchRequest_NetworkOptions>();
  @$core.pragma('dart2js:noInline')
  static LaunchRequest_NetworkOptions getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<LaunchRequest_NetworkOptions>(create);
  static LaunchRequest_NetworkOptions? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get id => $_getSZ(0);
  @$pb.TagNumber(1)
  set id($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasId() => $_has(0);
  @$pb.TagNumber(1)
  void clearId() => clearField(1);

  @$pb.TagNumber(2)
  LaunchRequest_NetworkOptions_Mode get mode => $_getN(1);
  @$pb.TagNumber(2)
  set mode(LaunchRequest_NetworkOptions_Mode v) { setField(2, v); }
  @$pb.TagNumber(2)
  $core.bool hasMode() => $_has(1);
  @$pb.TagNumber(2)
  void clearMode() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get macAddress => $_getSZ(2);
  @$pb.TagNumber(3)
  set macAddress($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasMacAddress() => $_has(2);
  @$pb.TagNumber(3)
  void clearMacAddress() => clearField(3);
}

class LaunchRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'LaunchRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceName')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'image')
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'kernelName')
    ..a<$core.int>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'numCores', $pb.PbFieldType.O3)
    ..aOS(5, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'memSize')
    ..aOS(6, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'diskSpace')
    ..aOS(7, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'timeZone')
    ..aOS(8, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'cloudInitUserData')
    ..aOS(9, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'remoteName')
    ..a<$core.int>(11, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..pc<LaunchRequest_NetworkOptions>(12, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'networkOptions', $pb.PbFieldType.PM, subBuilder: LaunchRequest_NetworkOptions.create)
    ..aOB(13, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'permissionToBridge')
    ..a<$core.int>(14, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'timeout', $pb.PbFieldType.O3)
    ..aOM<UserCredentials>(15, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'userCredentials', subBuilder: UserCredentials.create)
    ..hasRequiredFields = false
  ;

  LaunchRequest._() : super();
  factory LaunchRequest({
    $core.String? instanceName,
    $core.String? image,
    $core.String? kernelName,
    $core.int? numCores,
    $core.String? memSize,
    $core.String? diskSpace,
    $core.String? timeZone,
    $core.String? cloudInitUserData,
    $core.String? remoteName,
    $core.int? verbosityLevel,
    $core.Iterable<LaunchRequest_NetworkOptions>? networkOptions,
    $core.bool? permissionToBridge,
    $core.int? timeout,
    UserCredentials? userCredentials,
  }) {
    final _result = create();
    if (instanceName != null) {
      _result.instanceName = instanceName;
    }
    if (image != null) {
      _result.image = image;
    }
    if (kernelName != null) {
      _result.kernelName = kernelName;
    }
    if (numCores != null) {
      _result.numCores = numCores;
    }
    if (memSize != null) {
      _result.memSize = memSize;
    }
    if (diskSpace != null) {
      _result.diskSpace = diskSpace;
    }
    if (timeZone != null) {
      _result.timeZone = timeZone;
    }
    if (cloudInitUserData != null) {
      _result.cloudInitUserData = cloudInitUserData;
    }
    if (remoteName != null) {
      _result.remoteName = remoteName;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (networkOptions != null) {
      _result.networkOptions.addAll(networkOptions);
    }
    if (permissionToBridge != null) {
      _result.permissionToBridge = permissionToBridge;
    }
    if (timeout != null) {
      _result.timeout = timeout;
    }
    if (userCredentials != null) {
      _result.userCredentials = userCredentials;
    }
    return _result;
  }
  factory LaunchRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory LaunchRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  LaunchRequest clone() => LaunchRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  LaunchRequest copyWith(void Function(LaunchRequest) updates) => super.copyWith((message) => updates(message as LaunchRequest)) as LaunchRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static LaunchRequest create() => LaunchRequest._();
  LaunchRequest createEmptyInstance() => create();
  static $pb.PbList<LaunchRequest> createRepeated() => $pb.PbList<LaunchRequest>();
  @$core.pragma('dart2js:noInline')
  static LaunchRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<LaunchRequest>(create);
  static LaunchRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get instanceName => $_getSZ(0);
  @$pb.TagNumber(1)
  set instanceName($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceName() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceName() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get image => $_getSZ(1);
  @$pb.TagNumber(2)
  set image($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasImage() => $_has(1);
  @$pb.TagNumber(2)
  void clearImage() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get kernelName => $_getSZ(2);
  @$pb.TagNumber(3)
  set kernelName($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasKernelName() => $_has(2);
  @$pb.TagNumber(3)
  void clearKernelName() => clearField(3);

  @$pb.TagNumber(4)
  $core.int get numCores => $_getIZ(3);
  @$pb.TagNumber(4)
  set numCores($core.int v) { $_setSignedInt32(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasNumCores() => $_has(3);
  @$pb.TagNumber(4)
  void clearNumCores() => clearField(4);

  @$pb.TagNumber(5)
  $core.String get memSize => $_getSZ(4);
  @$pb.TagNumber(5)
  set memSize($core.String v) { $_setString(4, v); }
  @$pb.TagNumber(5)
  $core.bool hasMemSize() => $_has(4);
  @$pb.TagNumber(5)
  void clearMemSize() => clearField(5);

  @$pb.TagNumber(6)
  $core.String get diskSpace => $_getSZ(5);
  @$pb.TagNumber(6)
  set diskSpace($core.String v) { $_setString(5, v); }
  @$pb.TagNumber(6)
  $core.bool hasDiskSpace() => $_has(5);
  @$pb.TagNumber(6)
  void clearDiskSpace() => clearField(6);

  @$pb.TagNumber(7)
  $core.String get timeZone => $_getSZ(6);
  @$pb.TagNumber(7)
  set timeZone($core.String v) { $_setString(6, v); }
  @$pb.TagNumber(7)
  $core.bool hasTimeZone() => $_has(6);
  @$pb.TagNumber(7)
  void clearTimeZone() => clearField(7);

  @$pb.TagNumber(8)
  $core.String get cloudInitUserData => $_getSZ(7);
  @$pb.TagNumber(8)
  set cloudInitUserData($core.String v) { $_setString(7, v); }
  @$pb.TagNumber(8)
  $core.bool hasCloudInitUserData() => $_has(7);
  @$pb.TagNumber(8)
  void clearCloudInitUserData() => clearField(8);

  @$pb.TagNumber(9)
  $core.String get remoteName => $_getSZ(8);
  @$pb.TagNumber(9)
  set remoteName($core.String v) { $_setString(8, v); }
  @$pb.TagNumber(9)
  $core.bool hasRemoteName() => $_has(8);
  @$pb.TagNumber(9)
  void clearRemoteName() => clearField(9);

  @$pb.TagNumber(11)
  $core.int get verbosityLevel => $_getIZ(9);
  @$pb.TagNumber(11)
  set verbosityLevel($core.int v) { $_setSignedInt32(9, v); }
  @$pb.TagNumber(11)
  $core.bool hasVerbosityLevel() => $_has(9);
  @$pb.TagNumber(11)
  void clearVerbosityLevel() => clearField(11);

  @$pb.TagNumber(12)
  $core.List<LaunchRequest_NetworkOptions> get networkOptions => $_getList(10);

  @$pb.TagNumber(13)
  $core.bool get permissionToBridge => $_getBF(11);
  @$pb.TagNumber(13)
  set permissionToBridge($core.bool v) { $_setBool(11, v); }
  @$pb.TagNumber(13)
  $core.bool hasPermissionToBridge() => $_has(11);
  @$pb.TagNumber(13)
  void clearPermissionToBridge() => clearField(13);

  @$pb.TagNumber(14)
  $core.int get timeout => $_getIZ(12);
  @$pb.TagNumber(14)
  set timeout($core.int v) { $_setSignedInt32(12, v); }
  @$pb.TagNumber(14)
  $core.bool hasTimeout() => $_has(12);
  @$pb.TagNumber(14)
  void clearTimeout() => clearField(14);

  @$pb.TagNumber(15)
  UserCredentials get userCredentials => $_getN(13);
  @$pb.TagNumber(15)
  set userCredentials(UserCredentials v) { setField(15, v); }
  @$pb.TagNumber(15)
  $core.bool hasUserCredentials() => $_has(13);
  @$pb.TagNumber(15)
  void clearUserCredentials() => clearField(15);
  @$pb.TagNumber(15)
  UserCredentials ensureUserCredentials() => $_ensure(13);
}

class LaunchError extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'LaunchError', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pc<LaunchError_ErrorCodes>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'errorCodes', $pb.PbFieldType.KE, valueOf: LaunchError_ErrorCodes.valueOf, enumValues: LaunchError_ErrorCodes.values, defaultEnumValue: LaunchError_ErrorCodes.OK)
    ..hasRequiredFields = false
  ;

  LaunchError._() : super();
  factory LaunchError({
    $core.Iterable<LaunchError_ErrorCodes>? errorCodes,
  }) {
    final _result = create();
    if (errorCodes != null) {
      _result.errorCodes.addAll(errorCodes);
    }
    return _result;
  }
  factory LaunchError.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory LaunchError.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  LaunchError clone() => LaunchError()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  LaunchError copyWith(void Function(LaunchError) updates) => super.copyWith((message) => updates(message as LaunchError)) as LaunchError; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static LaunchError create() => LaunchError._();
  LaunchError createEmptyInstance() => create();
  static $pb.PbList<LaunchError> createRepeated() => $pb.PbList<LaunchError>();
  @$core.pragma('dart2js:noInline')
  static LaunchError getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<LaunchError>(create);
  static LaunchError? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<LaunchError_ErrorCodes> get errorCodes => $_getList(0);
}

class LaunchProgress extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'LaunchProgress', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..e<LaunchProgress_ProgressTypes>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'type', $pb.PbFieldType.OE, defaultOrMaker: LaunchProgress_ProgressTypes.IMAGE, valueOf: LaunchProgress_ProgressTypes.valueOf, enumValues: LaunchProgress_ProgressTypes.values)
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'percentComplete')
    ..hasRequiredFields = false
  ;

  LaunchProgress._() : super();
  factory LaunchProgress({
    LaunchProgress_ProgressTypes? type,
    $core.String? percentComplete,
  }) {
    final _result = create();
    if (type != null) {
      _result.type = type;
    }
    if (percentComplete != null) {
      _result.percentComplete = percentComplete;
    }
    return _result;
  }
  factory LaunchProgress.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory LaunchProgress.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  LaunchProgress clone() => LaunchProgress()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  LaunchProgress copyWith(void Function(LaunchProgress) updates) => super.copyWith((message) => updates(message as LaunchProgress)) as LaunchProgress; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static LaunchProgress create() => LaunchProgress._();
  LaunchProgress createEmptyInstance() => create();
  static $pb.PbList<LaunchProgress> createRepeated() => $pb.PbList<LaunchProgress>();
  @$core.pragma('dart2js:noInline')
  static LaunchProgress getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<LaunchProgress>(create);
  static LaunchProgress? _defaultInstance;

  @$pb.TagNumber(1)
  LaunchProgress_ProgressTypes get type => $_getN(0);
  @$pb.TagNumber(1)
  set type(LaunchProgress_ProgressTypes v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasType() => $_has(0);
  @$pb.TagNumber(1)
  void clearType() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get percentComplete => $_getSZ(1);
  @$pb.TagNumber(2)
  set percentComplete($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasPercentComplete() => $_has(1);
  @$pb.TagNumber(2)
  void clearPercentComplete() => clearField(2);
}

class UpdateInfo extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'UpdateInfo', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'version')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'url')
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'title')
    ..aOS(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'description')
    ..hasRequiredFields = false
  ;

  UpdateInfo._() : super();
  factory UpdateInfo({
    $core.String? version,
    $core.String? url,
    $core.String? title,
    $core.String? description,
  }) {
    final _result = create();
    if (version != null) {
      _result.version = version;
    }
    if (url != null) {
      _result.url = url;
    }
    if (title != null) {
      _result.title = title;
    }
    if (description != null) {
      _result.description = description;
    }
    return _result;
  }
  factory UpdateInfo.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory UpdateInfo.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  UpdateInfo clone() => UpdateInfo()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  UpdateInfo copyWith(void Function(UpdateInfo) updates) => super.copyWith((message) => updates(message as UpdateInfo)) as UpdateInfo; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static UpdateInfo create() => UpdateInfo._();
  UpdateInfo createEmptyInstance() => create();
  static $pb.PbList<UpdateInfo> createRepeated() => $pb.PbList<UpdateInfo>();
  @$core.pragma('dart2js:noInline')
  static UpdateInfo getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<UpdateInfo>(create);
  static UpdateInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get version => $_getSZ(0);
  @$pb.TagNumber(1)
  set version($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVersion() => $_has(0);
  @$pb.TagNumber(1)
  void clearVersion() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get url => $_getSZ(1);
  @$pb.TagNumber(2)
  set url($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasUrl() => $_has(1);
  @$pb.TagNumber(2)
  void clearUrl() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get title => $_getSZ(2);
  @$pb.TagNumber(3)
  set title($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasTitle() => $_has(2);
  @$pb.TagNumber(3)
  void clearTitle() => clearField(3);

  @$pb.TagNumber(4)
  $core.String get description => $_getSZ(3);
  @$pb.TagNumber(4)
  set description($core.String v) { $_setString(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasDescription() => $_has(3);
  @$pb.TagNumber(4)
  void clearDescription() => clearField(4);
}

class LaunchReply_Alias extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'LaunchReply.Alias', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'name')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instance')
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'command')
    ..aOS(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'workingDirectory')
    ..hasRequiredFields = false
  ;

  LaunchReply_Alias._() : super();
  factory LaunchReply_Alias({
    $core.String? name,
    $core.String? instance,
    $core.String? command,
    $core.String? workingDirectory,
  }) {
    final _result = create();
    if (name != null) {
      _result.name = name;
    }
    if (instance != null) {
      _result.instance = instance;
    }
    if (command != null) {
      _result.command = command;
    }
    if (workingDirectory != null) {
      _result.workingDirectory = workingDirectory;
    }
    return _result;
  }
  factory LaunchReply_Alias.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory LaunchReply_Alias.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  LaunchReply_Alias clone() => LaunchReply_Alias()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  LaunchReply_Alias copyWith(void Function(LaunchReply_Alias) updates) => super.copyWith((message) => updates(message as LaunchReply_Alias)) as LaunchReply_Alias; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static LaunchReply_Alias create() => LaunchReply_Alias._();
  LaunchReply_Alias createEmptyInstance() => create();
  static $pb.PbList<LaunchReply_Alias> createRepeated() => $pb.PbList<LaunchReply_Alias>();
  @$core.pragma('dart2js:noInline')
  static LaunchReply_Alias getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<LaunchReply_Alias>(create);
  static LaunchReply_Alias? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get instance => $_getSZ(1);
  @$pb.TagNumber(2)
  set instance($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasInstance() => $_has(1);
  @$pb.TagNumber(2)
  void clearInstance() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get command => $_getSZ(2);
  @$pb.TagNumber(3)
  set command($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasCommand() => $_has(2);
  @$pb.TagNumber(3)
  void clearCommand() => clearField(3);

  @$pb.TagNumber(4)
  $core.String get workingDirectory => $_getSZ(3);
  @$pb.TagNumber(4)
  set workingDirectory($core.String v) { $_setString(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasWorkingDirectory() => $_has(3);
  @$pb.TagNumber(4)
  void clearWorkingDirectory() => clearField(4);
}

enum LaunchReply_CreateOneof {
  vmInstanceName, 
  launchProgress, 
  createMessage, 
  notSet
}

class LaunchReply extends $pb.GeneratedMessage {
  static const $core.Map<$core.int, LaunchReply_CreateOneof> _LaunchReply_CreateOneofByTag = {
    1 : LaunchReply_CreateOneof.vmInstanceName,
    2 : LaunchReply_CreateOneof.launchProgress,
    3 : LaunchReply_CreateOneof.createMessage,
    0 : LaunchReply_CreateOneof.notSet
  };
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'LaunchReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..oo(0, [1, 2, 3])
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'vmInstanceName')
    ..aOM<LaunchProgress>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'launchProgress', subBuilder: LaunchProgress.create)
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'createMessage')
    ..aOS(6, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOM<UpdateInfo>(7, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'updateInfo', subBuilder: UpdateInfo.create)
    ..aOS(8, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'replyMessage')
    ..pPS(9, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'netsNeedBridging')
    ..pc<LaunchReply_Alias>(10, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'aliasesToBeCreated', $pb.PbFieldType.PM, subBuilder: LaunchReply_Alias.create)
    ..pPS(11, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'workspacesToBeCreated')
    ..aOB(12, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'credentialsRequested')
    ..hasRequiredFields = false
  ;

  LaunchReply._() : super();
  factory LaunchReply({
    $core.String? vmInstanceName,
    LaunchProgress? launchProgress,
    $core.String? createMessage,
    $core.String? logLine,
    UpdateInfo? updateInfo,
    $core.String? replyMessage,
    $core.Iterable<$core.String>? netsNeedBridging,
    $core.Iterable<LaunchReply_Alias>? aliasesToBeCreated,
    $core.Iterable<$core.String>? workspacesToBeCreated,
    $core.bool? credentialsRequested,
  }) {
    final _result = create();
    if (vmInstanceName != null) {
      _result.vmInstanceName = vmInstanceName;
    }
    if (launchProgress != null) {
      _result.launchProgress = launchProgress;
    }
    if (createMessage != null) {
      _result.createMessage = createMessage;
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (updateInfo != null) {
      _result.updateInfo = updateInfo;
    }
    if (replyMessage != null) {
      _result.replyMessage = replyMessage;
    }
    if (netsNeedBridging != null) {
      _result.netsNeedBridging.addAll(netsNeedBridging);
    }
    if (aliasesToBeCreated != null) {
      _result.aliasesToBeCreated.addAll(aliasesToBeCreated);
    }
    if (workspacesToBeCreated != null) {
      _result.workspacesToBeCreated.addAll(workspacesToBeCreated);
    }
    if (credentialsRequested != null) {
      _result.credentialsRequested = credentialsRequested;
    }
    return _result;
  }
  factory LaunchReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory LaunchReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  LaunchReply clone() => LaunchReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  LaunchReply copyWith(void Function(LaunchReply) updates) => super.copyWith((message) => updates(message as LaunchReply)) as LaunchReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static LaunchReply create() => LaunchReply._();
  LaunchReply createEmptyInstance() => create();
  static $pb.PbList<LaunchReply> createRepeated() => $pb.PbList<LaunchReply>();
  @$core.pragma('dart2js:noInline')
  static LaunchReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<LaunchReply>(create);
  static LaunchReply? _defaultInstance;

  LaunchReply_CreateOneof whichCreateOneof() => _LaunchReply_CreateOneofByTag[$_whichOneof(0)]!;
  void clearCreateOneof() => clearField($_whichOneof(0));

  @$pb.TagNumber(1)
  $core.String get vmInstanceName => $_getSZ(0);
  @$pb.TagNumber(1)
  set vmInstanceName($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVmInstanceName() => $_has(0);
  @$pb.TagNumber(1)
  void clearVmInstanceName() => clearField(1);

  @$pb.TagNumber(2)
  LaunchProgress get launchProgress => $_getN(1);
  @$pb.TagNumber(2)
  set launchProgress(LaunchProgress v) { setField(2, v); }
  @$pb.TagNumber(2)
  $core.bool hasLaunchProgress() => $_has(1);
  @$pb.TagNumber(2)
  void clearLaunchProgress() => clearField(2);
  @$pb.TagNumber(2)
  LaunchProgress ensureLaunchProgress() => $_ensure(1);

  @$pb.TagNumber(3)
  $core.String get createMessage => $_getSZ(2);
  @$pb.TagNumber(3)
  set createMessage($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasCreateMessage() => $_has(2);
  @$pb.TagNumber(3)
  void clearCreateMessage() => clearField(3);

  @$pb.TagNumber(6)
  $core.String get logLine => $_getSZ(3);
  @$pb.TagNumber(6)
  set logLine($core.String v) { $_setString(3, v); }
  @$pb.TagNumber(6)
  $core.bool hasLogLine() => $_has(3);
  @$pb.TagNumber(6)
  void clearLogLine() => clearField(6);

  @$pb.TagNumber(7)
  UpdateInfo get updateInfo => $_getN(4);
  @$pb.TagNumber(7)
  set updateInfo(UpdateInfo v) { setField(7, v); }
  @$pb.TagNumber(7)
  $core.bool hasUpdateInfo() => $_has(4);
  @$pb.TagNumber(7)
  void clearUpdateInfo() => clearField(7);
  @$pb.TagNumber(7)
  UpdateInfo ensureUpdateInfo() => $_ensure(4);

  @$pb.TagNumber(8)
  $core.String get replyMessage => $_getSZ(5);
  @$pb.TagNumber(8)
  set replyMessage($core.String v) { $_setString(5, v); }
  @$pb.TagNumber(8)
  $core.bool hasReplyMessage() => $_has(5);
  @$pb.TagNumber(8)
  void clearReplyMessage() => clearField(8);

  @$pb.TagNumber(9)
  $core.List<$core.String> get netsNeedBridging => $_getList(6);

  @$pb.TagNumber(10)
  $core.List<LaunchReply_Alias> get aliasesToBeCreated => $_getList(7);

  @$pb.TagNumber(11)
  $core.List<$core.String> get workspacesToBeCreated => $_getList(8);

  @$pb.TagNumber(12)
  $core.bool get credentialsRequested => $_getBF(9);
  @$pb.TagNumber(12)
  set credentialsRequested($core.bool v) { $_setBool(9, v); }
  @$pb.TagNumber(12)
  $core.bool hasCredentialsRequested() => $_has(9);
  @$pb.TagNumber(12)
  void clearCredentialsRequested() => clearField(12);
}

class PurgeRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'PurgeRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  PurgeRequest._() : super();
  factory PurgeRequest({
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory PurgeRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory PurgeRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  PurgeRequest clone() => PurgeRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  PurgeRequest copyWith(void Function(PurgeRequest) updates) => super.copyWith((message) => updates(message as PurgeRequest)) as PurgeRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static PurgeRequest create() => PurgeRequest._();
  PurgeRequest createEmptyInstance() => create();
  static $pb.PbList<PurgeRequest> createRepeated() => $pb.PbList<PurgeRequest>();
  @$core.pragma('dart2js:noInline')
  static PurgeRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<PurgeRequest>(create);
  static PurgeRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get verbosityLevel => $_getIZ(0);
  @$pb.TagNumber(1)
  set verbosityLevel($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVerbosityLevel() => $_has(0);
  @$pb.TagNumber(1)
  void clearVerbosityLevel() => clearField(1);
}

class PurgeReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'PurgeReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..pPS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'purgedInstances')
    ..hasRequiredFields = false
  ;

  PurgeReply._() : super();
  factory PurgeReply({
    $core.String? logLine,
    $core.Iterable<$core.String>? purgedInstances,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (purgedInstances != null) {
      _result.purgedInstances.addAll(purgedInstances);
    }
    return _result;
  }
  factory PurgeReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory PurgeReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  PurgeReply clone() => PurgeReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  PurgeReply copyWith(void Function(PurgeReply) updates) => super.copyWith((message) => updates(message as PurgeReply)) as PurgeReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static PurgeReply create() => PurgeReply._();
  PurgeReply createEmptyInstance() => create();
  static $pb.PbList<PurgeReply> createRepeated() => $pb.PbList<PurgeReply>();
  @$core.pragma('dart2js:noInline')
  static PurgeReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<PurgeReply>(create);
  static PurgeReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.String> get purgedInstances => $_getList(1);
}

class FindRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'FindRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'searchString')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'remoteName')
    ..a<$core.int>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..aOB(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'allowUnsupported')
    ..aOB(5, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'showImages')
    ..aOB(6, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'showBlueprints')
    ..hasRequiredFields = false
  ;

  FindRequest._() : super();
  factory FindRequest({
    $core.String? searchString,
    $core.String? remoteName,
    $core.int? verbosityLevel,
    $core.bool? allowUnsupported,
    $core.bool? showImages,
    $core.bool? showBlueprints,
  }) {
    final _result = create();
    if (searchString != null) {
      _result.searchString = searchString;
    }
    if (remoteName != null) {
      _result.remoteName = remoteName;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (allowUnsupported != null) {
      _result.allowUnsupported = allowUnsupported;
    }
    if (showImages != null) {
      _result.showImages = showImages;
    }
    if (showBlueprints != null) {
      _result.showBlueprints = showBlueprints;
    }
    return _result;
  }
  factory FindRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory FindRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  FindRequest clone() => FindRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  FindRequest copyWith(void Function(FindRequest) updates) => super.copyWith((message) => updates(message as FindRequest)) as FindRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static FindRequest create() => FindRequest._();
  FindRequest createEmptyInstance() => create();
  static $pb.PbList<FindRequest> createRepeated() => $pb.PbList<FindRequest>();
  @$core.pragma('dart2js:noInline')
  static FindRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<FindRequest>(create);
  static FindRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get searchString => $_getSZ(0);
  @$pb.TagNumber(1)
  set searchString($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasSearchString() => $_has(0);
  @$pb.TagNumber(1)
  void clearSearchString() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get remoteName => $_getSZ(1);
  @$pb.TagNumber(2)
  set remoteName($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasRemoteName() => $_has(1);
  @$pb.TagNumber(2)
  void clearRemoteName() => clearField(2);

  @$pb.TagNumber(3)
  $core.int get verbosityLevel => $_getIZ(2);
  @$pb.TagNumber(3)
  set verbosityLevel($core.int v) { $_setSignedInt32(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasVerbosityLevel() => $_has(2);
  @$pb.TagNumber(3)
  void clearVerbosityLevel() => clearField(3);

  @$pb.TagNumber(4)
  $core.bool get allowUnsupported => $_getBF(3);
  @$pb.TagNumber(4)
  set allowUnsupported($core.bool v) { $_setBool(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasAllowUnsupported() => $_has(3);
  @$pb.TagNumber(4)
  void clearAllowUnsupported() => clearField(4);

  @$pb.TagNumber(5)
  $core.bool get showImages => $_getBF(4);
  @$pb.TagNumber(5)
  set showImages($core.bool v) { $_setBool(4, v); }
  @$pb.TagNumber(5)
  $core.bool hasShowImages() => $_has(4);
  @$pb.TagNumber(5)
  void clearShowImages() => clearField(5);

  @$pb.TagNumber(6)
  $core.bool get showBlueprints => $_getBF(5);
  @$pb.TagNumber(6)
  set showBlueprints($core.bool v) { $_setBool(5, v); }
  @$pb.TagNumber(6)
  $core.bool hasShowBlueprints() => $_has(5);
  @$pb.TagNumber(6)
  void clearShowBlueprints() => clearField(6);
}

class FindReply_AliasInfo extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'FindReply.AliasInfo', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'remoteName')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'alias')
    ..hasRequiredFields = false
  ;

  FindReply_AliasInfo._() : super();
  factory FindReply_AliasInfo({
    $core.String? remoteName,
    $core.String? alias,
  }) {
    final _result = create();
    if (remoteName != null) {
      _result.remoteName = remoteName;
    }
    if (alias != null) {
      _result.alias = alias;
    }
    return _result;
  }
  factory FindReply_AliasInfo.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory FindReply_AliasInfo.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  FindReply_AliasInfo clone() => FindReply_AliasInfo()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  FindReply_AliasInfo copyWith(void Function(FindReply_AliasInfo) updates) => super.copyWith((message) => updates(message as FindReply_AliasInfo)) as FindReply_AliasInfo; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static FindReply_AliasInfo create() => FindReply_AliasInfo._();
  FindReply_AliasInfo createEmptyInstance() => create();
  static $pb.PbList<FindReply_AliasInfo> createRepeated() => $pb.PbList<FindReply_AliasInfo>();
  @$core.pragma('dart2js:noInline')
  static FindReply_AliasInfo getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<FindReply_AliasInfo>(create);
  static FindReply_AliasInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get remoteName => $_getSZ(0);
  @$pb.TagNumber(1)
  set remoteName($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasRemoteName() => $_has(0);
  @$pb.TagNumber(1)
  void clearRemoteName() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get alias => $_getSZ(1);
  @$pb.TagNumber(2)
  set alias($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasAlias() => $_has(1);
  @$pb.TagNumber(2)
  void clearAlias() => clearField(2);
}

class FindReply_ImageInfo extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'FindReply.ImageInfo', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'os')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'release')
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'version')
    ..pc<FindReply_AliasInfo>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'aliasesInfo', $pb.PbFieldType.PM, subBuilder: FindReply_AliasInfo.create)
    ..hasRequiredFields = false
  ;

  FindReply_ImageInfo._() : super();
  factory FindReply_ImageInfo({
    $core.String? os,
    $core.String? release,
    $core.String? version,
    $core.Iterable<FindReply_AliasInfo>? aliasesInfo,
  }) {
    final _result = create();
    if (os != null) {
      _result.os = os;
    }
    if (release != null) {
      _result.release = release;
    }
    if (version != null) {
      _result.version = version;
    }
    if (aliasesInfo != null) {
      _result.aliasesInfo.addAll(aliasesInfo);
    }
    return _result;
  }
  factory FindReply_ImageInfo.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory FindReply_ImageInfo.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  FindReply_ImageInfo clone() => FindReply_ImageInfo()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  FindReply_ImageInfo copyWith(void Function(FindReply_ImageInfo) updates) => super.copyWith((message) => updates(message as FindReply_ImageInfo)) as FindReply_ImageInfo; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static FindReply_ImageInfo create() => FindReply_ImageInfo._();
  FindReply_ImageInfo createEmptyInstance() => create();
  static $pb.PbList<FindReply_ImageInfo> createRepeated() => $pb.PbList<FindReply_ImageInfo>();
  @$core.pragma('dart2js:noInline')
  static FindReply_ImageInfo getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<FindReply_ImageInfo>(create);
  static FindReply_ImageInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get os => $_getSZ(0);
  @$pb.TagNumber(1)
  set os($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasOs() => $_has(0);
  @$pb.TagNumber(1)
  void clearOs() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get release => $_getSZ(1);
  @$pb.TagNumber(2)
  set release($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasRelease() => $_has(1);
  @$pb.TagNumber(2)
  void clearRelease() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get version => $_getSZ(2);
  @$pb.TagNumber(3)
  set version($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasVersion() => $_has(2);
  @$pb.TagNumber(3)
  void clearVersion() => clearField(3);

  @$pb.TagNumber(4)
  $core.List<FindReply_AliasInfo> get aliasesInfo => $_getList(3);
}

class FindReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'FindReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOB(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'showImages')
    ..aOB(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'showBlueprints')
    ..pc<FindReply_ImageInfo>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'imagesInfo', $pb.PbFieldType.PM, subBuilder: FindReply_ImageInfo.create)
    ..pc<FindReply_ImageInfo>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'blueprintsInfo', $pb.PbFieldType.PM, subBuilder: FindReply_ImageInfo.create)
    ..aOS(5, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  FindReply._() : super();
  factory FindReply({
    $core.bool? showImages,
    $core.bool? showBlueprints,
    $core.Iterable<FindReply_ImageInfo>? imagesInfo,
    $core.Iterable<FindReply_ImageInfo>? blueprintsInfo,
    $core.String? logLine,
  }) {
    final _result = create();
    if (showImages != null) {
      _result.showImages = showImages;
    }
    if (showBlueprints != null) {
      _result.showBlueprints = showBlueprints;
    }
    if (imagesInfo != null) {
      _result.imagesInfo.addAll(imagesInfo);
    }
    if (blueprintsInfo != null) {
      _result.blueprintsInfo.addAll(blueprintsInfo);
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory FindReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory FindReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  FindReply clone() => FindReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  FindReply copyWith(void Function(FindReply) updates) => super.copyWith((message) => updates(message as FindReply)) as FindReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static FindReply create() => FindReply._();
  FindReply createEmptyInstance() => create();
  static $pb.PbList<FindReply> createRepeated() => $pb.PbList<FindReply>();
  @$core.pragma('dart2js:noInline')
  static FindReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<FindReply>(create);
  static FindReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get showImages => $_getBF(0);
  @$pb.TagNumber(1)
  set showImages($core.bool v) { $_setBool(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasShowImages() => $_has(0);
  @$pb.TagNumber(1)
  void clearShowImages() => clearField(1);

  @$pb.TagNumber(2)
  $core.bool get showBlueprints => $_getBF(1);
  @$pb.TagNumber(2)
  set showBlueprints($core.bool v) { $_setBool(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasShowBlueprints() => $_has(1);
  @$pb.TagNumber(2)
  void clearShowBlueprints() => clearField(2);

  @$pb.TagNumber(3)
  $core.List<FindReply_ImageInfo> get imagesInfo => $_getList(2);

  @$pb.TagNumber(4)
  $core.List<FindReply_ImageInfo> get blueprintsInfo => $_getList(3);

  @$pb.TagNumber(5)
  $core.String get logLine => $_getSZ(4);
  @$pb.TagNumber(5)
  set logLine($core.String v) { $_setString(4, v); }
  @$pb.TagNumber(5)
  $core.bool hasLogLine() => $_has(4);
  @$pb.TagNumber(5)
  void clearLogLine() => clearField(5);
}

class InstanceNames extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'InstanceNames', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pPS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceName')
    ..hasRequiredFields = false
  ;

  InstanceNames._() : super();
  factory InstanceNames({
    $core.Iterable<$core.String>? instanceName,
  }) {
    final _result = create();
    if (instanceName != null) {
      _result.instanceName.addAll(instanceName);
    }
    return _result;
  }
  factory InstanceNames.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory InstanceNames.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  InstanceNames clone() => InstanceNames()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  InstanceNames copyWith(void Function(InstanceNames) updates) => super.copyWith((message) => updates(message as InstanceNames)) as InstanceNames; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static InstanceNames create() => InstanceNames._();
  InstanceNames createEmptyInstance() => create();
  static $pb.PbList<InstanceNames> createRepeated() => $pb.PbList<InstanceNames>();
  @$core.pragma('dart2js:noInline')
  static InstanceNames getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<InstanceNames>(create);
  static InstanceNames? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.String> get instanceName => $_getList(0);
}

class InfoRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'InfoRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..aOB(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'noRuntimeInformation')
    ..hasRequiredFields = false
  ;

  InfoRequest._() : super();
  factory InfoRequest({
    InstanceNames? instanceNames,
    $core.int? verbosityLevel,
    $core.bool? noRuntimeInformation,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (noRuntimeInformation != null) {
      _result.noRuntimeInformation = noRuntimeInformation;
    }
    return _result;
  }
  factory InfoRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory InfoRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  InfoRequest clone() => InfoRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  InfoRequest copyWith(void Function(InfoRequest) updates) => super.copyWith((message) => updates(message as InfoRequest)) as InfoRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static InfoRequest create() => InfoRequest._();
  InfoRequest createEmptyInstance() => create();
  static $pb.PbList<InfoRequest> createRepeated() => $pb.PbList<InfoRequest>();
  @$core.pragma('dart2js:noInline')
  static InfoRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<InfoRequest>(create);
  static InfoRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);

  @$pb.TagNumber(3)
  $core.bool get noRuntimeInformation => $_getBF(2);
  @$pb.TagNumber(3)
  set noRuntimeInformation($core.bool v) { $_setBool(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasNoRuntimeInformation() => $_has(2);
  @$pb.TagNumber(3)
  void clearNoRuntimeInformation() => clearField(3);
}

class IdMap extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'IdMap', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'hostId', $pb.PbFieldType.O3)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceId', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  IdMap._() : super();
  factory IdMap({
    $core.int? hostId,
    $core.int? instanceId,
  }) {
    final _result = create();
    if (hostId != null) {
      _result.hostId = hostId;
    }
    if (instanceId != null) {
      _result.instanceId = instanceId;
    }
    return _result;
  }
  factory IdMap.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory IdMap.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  IdMap clone() => IdMap()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  IdMap copyWith(void Function(IdMap) updates) => super.copyWith((message) => updates(message as IdMap)) as IdMap; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static IdMap create() => IdMap._();
  IdMap createEmptyInstance() => create();
  static $pb.PbList<IdMap> createRepeated() => $pb.PbList<IdMap>();
  @$core.pragma('dart2js:noInline')
  static IdMap getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<IdMap>(create);
  static IdMap? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get hostId => $_getIZ(0);
  @$pb.TagNumber(1)
  set hostId($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasHostId() => $_has(0);
  @$pb.TagNumber(1)
  void clearHostId() => clearField(1);

  @$pb.TagNumber(2)
  $core.int get instanceId => $_getIZ(1);
  @$pb.TagNumber(2)
  set instanceId($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasInstanceId() => $_has(1);
  @$pb.TagNumber(2)
  void clearInstanceId() => clearField(2);
}

class MountMaps extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'MountMaps', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pc<IdMap>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'uidMappings', $pb.PbFieldType.PM, subBuilder: IdMap.create)
    ..pc<IdMap>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'gidMappings', $pb.PbFieldType.PM, subBuilder: IdMap.create)
    ..hasRequiredFields = false
  ;

  MountMaps._() : super();
  factory MountMaps({
    $core.Iterable<IdMap>? uidMappings,
    $core.Iterable<IdMap>? gidMappings,
  }) {
    final _result = create();
    if (uidMappings != null) {
      _result.uidMappings.addAll(uidMappings);
    }
    if (gidMappings != null) {
      _result.gidMappings.addAll(gidMappings);
    }
    return _result;
  }
  factory MountMaps.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory MountMaps.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  MountMaps clone() => MountMaps()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  MountMaps copyWith(void Function(MountMaps) updates) => super.copyWith((message) => updates(message as MountMaps)) as MountMaps; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static MountMaps create() => MountMaps._();
  MountMaps createEmptyInstance() => create();
  static $pb.PbList<MountMaps> createRepeated() => $pb.PbList<MountMaps>();
  @$core.pragma('dart2js:noInline')
  static MountMaps getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<MountMaps>(create);
  static MountMaps? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<IdMap> get uidMappings => $_getList(0);

  @$pb.TagNumber(2)
  $core.List<IdMap> get gidMappings => $_getList(1);
}

class MountInfo_MountPaths extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'MountInfo.MountPaths', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'sourcePath')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'targetPath')
    ..aOM<MountMaps>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'mountMaps', subBuilder: MountMaps.create)
    ..hasRequiredFields = false
  ;

  MountInfo_MountPaths._() : super();
  factory MountInfo_MountPaths({
    $core.String? sourcePath,
    $core.String? targetPath,
    MountMaps? mountMaps,
  }) {
    final _result = create();
    if (sourcePath != null) {
      _result.sourcePath = sourcePath;
    }
    if (targetPath != null) {
      _result.targetPath = targetPath;
    }
    if (mountMaps != null) {
      _result.mountMaps = mountMaps;
    }
    return _result;
  }
  factory MountInfo_MountPaths.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory MountInfo_MountPaths.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  MountInfo_MountPaths clone() => MountInfo_MountPaths()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  MountInfo_MountPaths copyWith(void Function(MountInfo_MountPaths) updates) => super.copyWith((message) => updates(message as MountInfo_MountPaths)) as MountInfo_MountPaths; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static MountInfo_MountPaths create() => MountInfo_MountPaths._();
  MountInfo_MountPaths createEmptyInstance() => create();
  static $pb.PbList<MountInfo_MountPaths> createRepeated() => $pb.PbList<MountInfo_MountPaths>();
  @$core.pragma('dart2js:noInline')
  static MountInfo_MountPaths getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<MountInfo_MountPaths>(create);
  static MountInfo_MountPaths? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get sourcePath => $_getSZ(0);
  @$pb.TagNumber(1)
  set sourcePath($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasSourcePath() => $_has(0);
  @$pb.TagNumber(1)
  void clearSourcePath() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get targetPath => $_getSZ(1);
  @$pb.TagNumber(2)
  set targetPath($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasTargetPath() => $_has(1);
  @$pb.TagNumber(2)
  void clearTargetPath() => clearField(2);

  @$pb.TagNumber(3)
  MountMaps get mountMaps => $_getN(2);
  @$pb.TagNumber(3)
  set mountMaps(MountMaps v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasMountMaps() => $_has(2);
  @$pb.TagNumber(3)
  void clearMountMaps() => clearField(3);
  @$pb.TagNumber(3)
  MountMaps ensureMountMaps() => $_ensure(2);
}

class MountInfo extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'MountInfo', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'longestPathLen', $pb.PbFieldType.OU3)
    ..pc<MountInfo_MountPaths>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'mountPaths', $pb.PbFieldType.PM, subBuilder: MountInfo_MountPaths.create)
    ..hasRequiredFields = false
  ;

  MountInfo._() : super();
  factory MountInfo({
    $core.int? longestPathLen,
    $core.Iterable<MountInfo_MountPaths>? mountPaths,
  }) {
    final _result = create();
    if (longestPathLen != null) {
      _result.longestPathLen = longestPathLen;
    }
    if (mountPaths != null) {
      _result.mountPaths.addAll(mountPaths);
    }
    return _result;
  }
  factory MountInfo.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory MountInfo.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  MountInfo clone() => MountInfo()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  MountInfo copyWith(void Function(MountInfo) updates) => super.copyWith((message) => updates(message as MountInfo)) as MountInfo; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static MountInfo create() => MountInfo._();
  MountInfo createEmptyInstance() => create();
  static $pb.PbList<MountInfo> createRepeated() => $pb.PbList<MountInfo>();
  @$core.pragma('dart2js:noInline')
  static MountInfo getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<MountInfo>(create);
  static MountInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get longestPathLen => $_getIZ(0);
  @$pb.TagNumber(1)
  set longestPathLen($core.int v) { $_setUnsignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLongestPathLen() => $_has(0);
  @$pb.TagNumber(1)
  void clearLongestPathLen() => clearField(1);

  @$pb.TagNumber(2)
  $core.List<MountInfo_MountPaths> get mountPaths => $_getList(1);
}

class InstanceStatus extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'InstanceStatus', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..e<InstanceStatus_Status>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'status', $pb.PbFieldType.OE, defaultOrMaker: InstanceStatus_Status.UNKNOWN, valueOf: InstanceStatus_Status.valueOf, enumValues: InstanceStatus_Status.values)
    ..hasRequiredFields = false
  ;

  InstanceStatus._() : super();
  factory InstanceStatus({
    InstanceStatus_Status? status,
  }) {
    final _result = create();
    if (status != null) {
      _result.status = status;
    }
    return _result;
  }
  factory InstanceStatus.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory InstanceStatus.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  InstanceStatus clone() => InstanceStatus()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  InstanceStatus copyWith(void Function(InstanceStatus) updates) => super.copyWith((message) => updates(message as InstanceStatus)) as InstanceStatus; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static InstanceStatus create() => InstanceStatus._();
  InstanceStatus createEmptyInstance() => create();
  static $pb.PbList<InstanceStatus> createRepeated() => $pb.PbList<InstanceStatus>();
  @$core.pragma('dart2js:noInline')
  static InstanceStatus getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<InstanceStatus>(create);
  static InstanceStatus? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceStatus_Status get status => $_getN(0);
  @$pb.TagNumber(1)
  set status(InstanceStatus_Status v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasStatus() => $_has(0);
  @$pb.TagNumber(1)
  void clearStatus() => clearField(1);
}

class InfoReply_Info extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'InfoReply.Info', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'name')
    ..aOM<InstanceStatus>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceStatus', subBuilder: InstanceStatus.create)
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'imageRelease')
    ..aOS(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'currentRelease')
    ..aOS(5, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'id')
    ..aOS(6, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'load')
    ..aOS(7, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'memoryUsage')
    ..aOS(8, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'memoryTotal')
    ..aOS(9, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'diskUsage')
    ..aOS(10, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'diskTotal')
    ..pPS(11, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'ipv4')
    ..pPS(12, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'ipv6')
    ..aOM<MountInfo>(13, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'mountInfo', subBuilder: MountInfo.create)
    ..aOS(14, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'cpuCount')
    ..hasRequiredFields = false
  ;

  InfoReply_Info._() : super();
  factory InfoReply_Info({
    $core.String? name,
    InstanceStatus? instanceStatus,
    $core.String? imageRelease,
    $core.String? currentRelease,
    $core.String? id,
    $core.String? load,
    $core.String? memoryUsage,
    $core.String? memoryTotal,
    $core.String? diskUsage,
    $core.String? diskTotal,
    $core.Iterable<$core.String>? ipv4,
    $core.Iterable<$core.String>? ipv6,
    MountInfo? mountInfo,
    $core.String? cpuCount,
  }) {
    final _result = create();
    if (name != null) {
      _result.name = name;
    }
    if (instanceStatus != null) {
      _result.instanceStatus = instanceStatus;
    }
    if (imageRelease != null) {
      _result.imageRelease = imageRelease;
    }
    if (currentRelease != null) {
      _result.currentRelease = currentRelease;
    }
    if (id != null) {
      _result.id = id;
    }
    if (load != null) {
      _result.load = load;
    }
    if (memoryUsage != null) {
      _result.memoryUsage = memoryUsage;
    }
    if (memoryTotal != null) {
      _result.memoryTotal = memoryTotal;
    }
    if (diskUsage != null) {
      _result.diskUsage = diskUsage;
    }
    if (diskTotal != null) {
      _result.diskTotal = diskTotal;
    }
    if (ipv4 != null) {
      _result.ipv4.addAll(ipv4);
    }
    if (ipv6 != null) {
      _result.ipv6.addAll(ipv6);
    }
    if (mountInfo != null) {
      _result.mountInfo = mountInfo;
    }
    if (cpuCount != null) {
      _result.cpuCount = cpuCount;
    }
    return _result;
  }
  factory InfoReply_Info.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory InfoReply_Info.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  InfoReply_Info clone() => InfoReply_Info()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  InfoReply_Info copyWith(void Function(InfoReply_Info) updates) => super.copyWith((message) => updates(message as InfoReply_Info)) as InfoReply_Info; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static InfoReply_Info create() => InfoReply_Info._();
  InfoReply_Info createEmptyInstance() => create();
  static $pb.PbList<InfoReply_Info> createRepeated() => $pb.PbList<InfoReply_Info>();
  @$core.pragma('dart2js:noInline')
  static InfoReply_Info getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<InfoReply_Info>(create);
  static InfoReply_Info? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => clearField(1);

  @$pb.TagNumber(2)
  InstanceStatus get instanceStatus => $_getN(1);
  @$pb.TagNumber(2)
  set instanceStatus(InstanceStatus v) { setField(2, v); }
  @$pb.TagNumber(2)
  $core.bool hasInstanceStatus() => $_has(1);
  @$pb.TagNumber(2)
  void clearInstanceStatus() => clearField(2);
  @$pb.TagNumber(2)
  InstanceStatus ensureInstanceStatus() => $_ensure(1);

  @$pb.TagNumber(3)
  $core.String get imageRelease => $_getSZ(2);
  @$pb.TagNumber(3)
  set imageRelease($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasImageRelease() => $_has(2);
  @$pb.TagNumber(3)
  void clearImageRelease() => clearField(3);

  @$pb.TagNumber(4)
  $core.String get currentRelease => $_getSZ(3);
  @$pb.TagNumber(4)
  set currentRelease($core.String v) { $_setString(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasCurrentRelease() => $_has(3);
  @$pb.TagNumber(4)
  void clearCurrentRelease() => clearField(4);

  @$pb.TagNumber(5)
  $core.String get id => $_getSZ(4);
  @$pb.TagNumber(5)
  set id($core.String v) { $_setString(4, v); }
  @$pb.TagNumber(5)
  $core.bool hasId() => $_has(4);
  @$pb.TagNumber(5)
  void clearId() => clearField(5);

  @$pb.TagNumber(6)
  $core.String get load => $_getSZ(5);
  @$pb.TagNumber(6)
  set load($core.String v) { $_setString(5, v); }
  @$pb.TagNumber(6)
  $core.bool hasLoad() => $_has(5);
  @$pb.TagNumber(6)
  void clearLoad() => clearField(6);

  @$pb.TagNumber(7)
  $core.String get memoryUsage => $_getSZ(6);
  @$pb.TagNumber(7)
  set memoryUsage($core.String v) { $_setString(6, v); }
  @$pb.TagNumber(7)
  $core.bool hasMemoryUsage() => $_has(6);
  @$pb.TagNumber(7)
  void clearMemoryUsage() => clearField(7);

  @$pb.TagNumber(8)
  $core.String get memoryTotal => $_getSZ(7);
  @$pb.TagNumber(8)
  set memoryTotal($core.String v) { $_setString(7, v); }
  @$pb.TagNumber(8)
  $core.bool hasMemoryTotal() => $_has(7);
  @$pb.TagNumber(8)
  void clearMemoryTotal() => clearField(8);

  @$pb.TagNumber(9)
  $core.String get diskUsage => $_getSZ(8);
  @$pb.TagNumber(9)
  set diskUsage($core.String v) { $_setString(8, v); }
  @$pb.TagNumber(9)
  $core.bool hasDiskUsage() => $_has(8);
  @$pb.TagNumber(9)
  void clearDiskUsage() => clearField(9);

  @$pb.TagNumber(10)
  $core.String get diskTotal => $_getSZ(9);
  @$pb.TagNumber(10)
  set diskTotal($core.String v) { $_setString(9, v); }
  @$pb.TagNumber(10)
  $core.bool hasDiskTotal() => $_has(9);
  @$pb.TagNumber(10)
  void clearDiskTotal() => clearField(10);

  @$pb.TagNumber(11)
  $core.List<$core.String> get ipv4 => $_getList(10);

  @$pb.TagNumber(12)
  $core.List<$core.String> get ipv6 => $_getList(11);

  @$pb.TagNumber(13)
  MountInfo get mountInfo => $_getN(12);
  @$pb.TagNumber(13)
  set mountInfo(MountInfo v) { setField(13, v); }
  @$pb.TagNumber(13)
  $core.bool hasMountInfo() => $_has(12);
  @$pb.TagNumber(13)
  void clearMountInfo() => clearField(13);
  @$pb.TagNumber(13)
  MountInfo ensureMountInfo() => $_ensure(12);

  @$pb.TagNumber(14)
  $core.String get cpuCount => $_getSZ(13);
  @$pb.TagNumber(14)
  set cpuCount($core.String v) { $_setString(13, v); }
  @$pb.TagNumber(14)
  $core.bool hasCpuCount() => $_has(13);
  @$pb.TagNumber(14)
  void clearCpuCount() => clearField(14);
}

class InfoReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'InfoReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pc<InfoReply_Info>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'info', $pb.PbFieldType.PM, subBuilder: InfoReply_Info.create)
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  InfoReply._() : super();
  factory InfoReply({
    $core.Iterable<InfoReply_Info>? info,
    $core.String? logLine,
  }) {
    final _result = create();
    if (info != null) {
      _result.info.addAll(info);
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory InfoReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory InfoReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  InfoReply clone() => InfoReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  InfoReply copyWith(void Function(InfoReply) updates) => super.copyWith((message) => updates(message as InfoReply)) as InfoReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static InfoReply create() => InfoReply._();
  InfoReply createEmptyInstance() => create();
  static $pb.PbList<InfoReply> createRepeated() => $pb.PbList<InfoReply>();
  @$core.pragma('dart2js:noInline')
  static InfoReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<InfoReply>(create);
  static InfoReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<InfoReply_Info> get info => $_getList(0);

  @$pb.TagNumber(2)
  $core.String get logLine => $_getSZ(1);
  @$pb.TagNumber(2)
  set logLine($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasLogLine() => $_has(1);
  @$pb.TagNumber(2)
  void clearLogLine() => clearField(2);
}

class ListRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'ListRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..aOB(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'requestIpv4')
    ..hasRequiredFields = false
  ;

  ListRequest._() : super();
  factory ListRequest({
    $core.int? verbosityLevel,
    $core.bool? requestIpv4,
  }) {
    final _result = create();
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (requestIpv4 != null) {
      _result.requestIpv4 = requestIpv4;
    }
    return _result;
  }
  factory ListRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory ListRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  ListRequest clone() => ListRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  ListRequest copyWith(void Function(ListRequest) updates) => super.copyWith((message) => updates(message as ListRequest)) as ListRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static ListRequest create() => ListRequest._();
  ListRequest createEmptyInstance() => create();
  static $pb.PbList<ListRequest> createRepeated() => $pb.PbList<ListRequest>();
  @$core.pragma('dart2js:noInline')
  static ListRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<ListRequest>(create);
  static ListRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get verbosityLevel => $_getIZ(0);
  @$pb.TagNumber(1)
  set verbosityLevel($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVerbosityLevel() => $_has(0);
  @$pb.TagNumber(1)
  void clearVerbosityLevel() => clearField(1);

  @$pb.TagNumber(2)
  $core.bool get requestIpv4 => $_getBF(1);
  @$pb.TagNumber(2)
  set requestIpv4($core.bool v) { $_setBool(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasRequestIpv4() => $_has(1);
  @$pb.TagNumber(2)
  void clearRequestIpv4() => clearField(2);
}

class ListVMInstance extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'ListVMInstance', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'name')
    ..aOM<InstanceStatus>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceStatus', subBuilder: InstanceStatus.create)
    ..pPS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'ipv4')
    ..pPS(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'ipv6')
    ..aOS(5, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'currentRelease')
    ..hasRequiredFields = false
  ;

  ListVMInstance._() : super();
  factory ListVMInstance({
    $core.String? name,
    InstanceStatus? instanceStatus,
    $core.Iterable<$core.String>? ipv4,
    $core.Iterable<$core.String>? ipv6,
    $core.String? currentRelease,
  }) {
    final _result = create();
    if (name != null) {
      _result.name = name;
    }
    if (instanceStatus != null) {
      _result.instanceStatus = instanceStatus;
    }
    if (ipv4 != null) {
      _result.ipv4.addAll(ipv4);
    }
    if (ipv6 != null) {
      _result.ipv6.addAll(ipv6);
    }
    if (currentRelease != null) {
      _result.currentRelease = currentRelease;
    }
    return _result;
  }
  factory ListVMInstance.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory ListVMInstance.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  ListVMInstance clone() => ListVMInstance()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  ListVMInstance copyWith(void Function(ListVMInstance) updates) => super.copyWith((message) => updates(message as ListVMInstance)) as ListVMInstance; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static ListVMInstance create() => ListVMInstance._();
  ListVMInstance createEmptyInstance() => create();
  static $pb.PbList<ListVMInstance> createRepeated() => $pb.PbList<ListVMInstance>();
  @$core.pragma('dart2js:noInline')
  static ListVMInstance getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<ListVMInstance>(create);
  static ListVMInstance? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => clearField(1);

  @$pb.TagNumber(2)
  InstanceStatus get instanceStatus => $_getN(1);
  @$pb.TagNumber(2)
  set instanceStatus(InstanceStatus v) { setField(2, v); }
  @$pb.TagNumber(2)
  $core.bool hasInstanceStatus() => $_has(1);
  @$pb.TagNumber(2)
  void clearInstanceStatus() => clearField(2);
  @$pb.TagNumber(2)
  InstanceStatus ensureInstanceStatus() => $_ensure(1);

  @$pb.TagNumber(3)
  $core.List<$core.String> get ipv4 => $_getList(2);

  @$pb.TagNumber(4)
  $core.List<$core.String> get ipv6 => $_getList(3);

  @$pb.TagNumber(5)
  $core.String get currentRelease => $_getSZ(4);
  @$pb.TagNumber(5)
  set currentRelease($core.String v) { $_setString(4, v); }
  @$pb.TagNumber(5)
  $core.bool hasCurrentRelease() => $_has(4);
  @$pb.TagNumber(5)
  void clearCurrentRelease() => clearField(5);
}

class ListReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'ListReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pc<ListVMInstance>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instances', $pb.PbFieldType.PM, subBuilder: ListVMInstance.create)
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOM<UpdateInfo>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'updateInfo', subBuilder: UpdateInfo.create)
    ..hasRequiredFields = false
  ;

  ListReply._() : super();
  factory ListReply({
    $core.Iterable<ListVMInstance>? instances,
    $core.String? logLine,
    UpdateInfo? updateInfo,
  }) {
    final _result = create();
    if (instances != null) {
      _result.instances.addAll(instances);
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (updateInfo != null) {
      _result.updateInfo = updateInfo;
    }
    return _result;
  }
  factory ListReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory ListReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  ListReply clone() => ListReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  ListReply copyWith(void Function(ListReply) updates) => super.copyWith((message) => updates(message as ListReply)) as ListReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static ListReply create() => ListReply._();
  ListReply createEmptyInstance() => create();
  static $pb.PbList<ListReply> createRepeated() => $pb.PbList<ListReply>();
  @$core.pragma('dart2js:noInline')
  static ListReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<ListReply>(create);
  static ListReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<ListVMInstance> get instances => $_getList(0);

  @$pb.TagNumber(2)
  $core.String get logLine => $_getSZ(1);
  @$pb.TagNumber(2)
  set logLine($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasLogLine() => $_has(1);
  @$pb.TagNumber(2)
  void clearLogLine() => clearField(2);

  @$pb.TagNumber(3)
  UpdateInfo get updateInfo => $_getN(2);
  @$pb.TagNumber(3)
  set updateInfo(UpdateInfo v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasUpdateInfo() => $_has(2);
  @$pb.TagNumber(3)
  void clearUpdateInfo() => clearField(3);
  @$pb.TagNumber(3)
  UpdateInfo ensureUpdateInfo() => $_ensure(2);
}

class NetworksRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'NetworksRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  NetworksRequest._() : super();
  factory NetworksRequest({
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory NetworksRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory NetworksRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  NetworksRequest clone() => NetworksRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  NetworksRequest copyWith(void Function(NetworksRequest) updates) => super.copyWith((message) => updates(message as NetworksRequest)) as NetworksRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static NetworksRequest create() => NetworksRequest._();
  NetworksRequest createEmptyInstance() => create();
  static $pb.PbList<NetworksRequest> createRepeated() => $pb.PbList<NetworksRequest>();
  @$core.pragma('dart2js:noInline')
  static NetworksRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<NetworksRequest>(create);
  static NetworksRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get verbosityLevel => $_getIZ(0);
  @$pb.TagNumber(1)
  set verbosityLevel($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVerbosityLevel() => $_has(0);
  @$pb.TagNumber(1)
  void clearVerbosityLevel() => clearField(1);
}

class NetInterface extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'NetInterface', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'name')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'type')
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'description')
    ..hasRequiredFields = false
  ;

  NetInterface._() : super();
  factory NetInterface({
    $core.String? name,
    $core.String? type,
    $core.String? description,
  }) {
    final _result = create();
    if (name != null) {
      _result.name = name;
    }
    if (type != null) {
      _result.type = type;
    }
    if (description != null) {
      _result.description = description;
    }
    return _result;
  }
  factory NetInterface.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory NetInterface.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  NetInterface clone() => NetInterface()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  NetInterface copyWith(void Function(NetInterface) updates) => super.copyWith((message) => updates(message as NetInterface)) as NetInterface; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static NetInterface create() => NetInterface._();
  NetInterface createEmptyInstance() => create();
  static $pb.PbList<NetInterface> createRepeated() => $pb.PbList<NetInterface>();
  @$core.pragma('dart2js:noInline')
  static NetInterface getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<NetInterface>(create);
  static NetInterface? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get type => $_getSZ(1);
  @$pb.TagNumber(2)
  set type($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasType() => $_has(1);
  @$pb.TagNumber(2)
  void clearType() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get description => $_getSZ(2);
  @$pb.TagNumber(3)
  set description($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasDescription() => $_has(2);
  @$pb.TagNumber(3)
  void clearDescription() => clearField(3);
}

class NetworksReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'NetworksReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pc<NetInterface>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'interfaces', $pb.PbFieldType.PM, subBuilder: NetInterface.create)
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOM<UpdateInfo>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'updateInfo', subBuilder: UpdateInfo.create)
    ..hasRequiredFields = false
  ;

  NetworksReply._() : super();
  factory NetworksReply({
    $core.Iterable<NetInterface>? interfaces,
    $core.String? logLine,
    UpdateInfo? updateInfo,
  }) {
    final _result = create();
    if (interfaces != null) {
      _result.interfaces.addAll(interfaces);
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (updateInfo != null) {
      _result.updateInfo = updateInfo;
    }
    return _result;
  }
  factory NetworksReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory NetworksReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  NetworksReply clone() => NetworksReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  NetworksReply copyWith(void Function(NetworksReply) updates) => super.copyWith((message) => updates(message as NetworksReply)) as NetworksReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static NetworksReply create() => NetworksReply._();
  NetworksReply createEmptyInstance() => create();
  static $pb.PbList<NetworksReply> createRepeated() => $pb.PbList<NetworksReply>();
  @$core.pragma('dart2js:noInline')
  static NetworksReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<NetworksReply>(create);
  static NetworksReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<NetInterface> get interfaces => $_getList(0);

  @$pb.TagNumber(2)
  $core.String get logLine => $_getSZ(1);
  @$pb.TagNumber(2)
  set logLine($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasLogLine() => $_has(1);
  @$pb.TagNumber(2)
  void clearLogLine() => clearField(2);

  @$pb.TagNumber(3)
  UpdateInfo get updateInfo => $_getN(2);
  @$pb.TagNumber(3)
  set updateInfo(UpdateInfo v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasUpdateInfo() => $_has(2);
  @$pb.TagNumber(3)
  void clearUpdateInfo() => clearField(3);
  @$pb.TagNumber(3)
  UpdateInfo ensureUpdateInfo() => $_ensure(2);
}

class TargetPathInfo extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'TargetPathInfo', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceName')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'targetPath')
    ..hasRequiredFields = false
  ;

  TargetPathInfo._() : super();
  factory TargetPathInfo({
    $core.String? instanceName,
    $core.String? targetPath,
  }) {
    final _result = create();
    if (instanceName != null) {
      _result.instanceName = instanceName;
    }
    if (targetPath != null) {
      _result.targetPath = targetPath;
    }
    return _result;
  }
  factory TargetPathInfo.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory TargetPathInfo.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  TargetPathInfo clone() => TargetPathInfo()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  TargetPathInfo copyWith(void Function(TargetPathInfo) updates) => super.copyWith((message) => updates(message as TargetPathInfo)) as TargetPathInfo; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static TargetPathInfo create() => TargetPathInfo._();
  TargetPathInfo createEmptyInstance() => create();
  static $pb.PbList<TargetPathInfo> createRepeated() => $pb.PbList<TargetPathInfo>();
  @$core.pragma('dart2js:noInline')
  static TargetPathInfo getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<TargetPathInfo>(create);
  static TargetPathInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get instanceName => $_getSZ(0);
  @$pb.TagNumber(1)
  set instanceName($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceName() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceName() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get targetPath => $_getSZ(1);
  @$pb.TagNumber(2)
  set targetPath($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasTargetPath() => $_has(1);
  @$pb.TagNumber(2)
  void clearTargetPath() => clearField(2);
}

class UserCredentials extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'UserCredentials', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'username')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'password')
    ..hasRequiredFields = false
  ;

  UserCredentials._() : super();
  factory UserCredentials({
    $core.String? username,
    $core.String? password,
  }) {
    final _result = create();
    if (username != null) {
      _result.username = username;
    }
    if (password != null) {
      _result.password = password;
    }
    return _result;
  }
  factory UserCredentials.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory UserCredentials.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  UserCredentials clone() => UserCredentials()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  UserCredentials copyWith(void Function(UserCredentials) updates) => super.copyWith((message) => updates(message as UserCredentials)) as UserCredentials; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static UserCredentials create() => UserCredentials._();
  UserCredentials createEmptyInstance() => create();
  static $pb.PbList<UserCredentials> createRepeated() => $pb.PbList<UserCredentials>();
  @$core.pragma('dart2js:noInline')
  static UserCredentials getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<UserCredentials>(create);
  static UserCredentials? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get username => $_getSZ(0);
  @$pb.TagNumber(1)
  set username($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasUsername() => $_has(0);
  @$pb.TagNumber(1)
  void clearUsername() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get password => $_getSZ(1);
  @$pb.TagNumber(2)
  set password($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasPassword() => $_has(1);
  @$pb.TagNumber(2)
  void clearPassword() => clearField(2);
}

class MountRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'MountRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'sourcePath')
    ..pc<TargetPathInfo>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'targetPaths', $pb.PbFieldType.PM, subBuilder: TargetPathInfo.create)
    ..aOM<MountMaps>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'mountMaps', subBuilder: MountMaps.create)
    ..a<$core.int>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..e<MountRequest_MountType>(5, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'mountType', $pb.PbFieldType.OE, defaultOrMaker: MountRequest_MountType.CLASSIC, valueOf: MountRequest_MountType.valueOf, enumValues: MountRequest_MountType.values)
    ..aOM<UserCredentials>(6, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'userCredentials', subBuilder: UserCredentials.create)
    ..hasRequiredFields = false
  ;

  MountRequest._() : super();
  factory MountRequest({
    $core.String? sourcePath,
    $core.Iterable<TargetPathInfo>? targetPaths,
    MountMaps? mountMaps,
    $core.int? verbosityLevel,
    MountRequest_MountType? mountType,
    UserCredentials? userCredentials,
  }) {
    final _result = create();
    if (sourcePath != null) {
      _result.sourcePath = sourcePath;
    }
    if (targetPaths != null) {
      _result.targetPaths.addAll(targetPaths);
    }
    if (mountMaps != null) {
      _result.mountMaps = mountMaps;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (mountType != null) {
      _result.mountType = mountType;
    }
    if (userCredentials != null) {
      _result.userCredentials = userCredentials;
    }
    return _result;
  }
  factory MountRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory MountRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  MountRequest clone() => MountRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  MountRequest copyWith(void Function(MountRequest) updates) => super.copyWith((message) => updates(message as MountRequest)) as MountRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static MountRequest create() => MountRequest._();
  MountRequest createEmptyInstance() => create();
  static $pb.PbList<MountRequest> createRepeated() => $pb.PbList<MountRequest>();
  @$core.pragma('dart2js:noInline')
  static MountRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<MountRequest>(create);
  static MountRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get sourcePath => $_getSZ(0);
  @$pb.TagNumber(1)
  set sourcePath($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasSourcePath() => $_has(0);
  @$pb.TagNumber(1)
  void clearSourcePath() => clearField(1);

  @$pb.TagNumber(2)
  $core.List<TargetPathInfo> get targetPaths => $_getList(1);

  @$pb.TagNumber(3)
  MountMaps get mountMaps => $_getN(2);
  @$pb.TagNumber(3)
  set mountMaps(MountMaps v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasMountMaps() => $_has(2);
  @$pb.TagNumber(3)
  void clearMountMaps() => clearField(3);
  @$pb.TagNumber(3)
  MountMaps ensureMountMaps() => $_ensure(2);

  @$pb.TagNumber(4)
  $core.int get verbosityLevel => $_getIZ(3);
  @$pb.TagNumber(4)
  set verbosityLevel($core.int v) { $_setSignedInt32(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasVerbosityLevel() => $_has(3);
  @$pb.TagNumber(4)
  void clearVerbosityLevel() => clearField(4);

  @$pb.TagNumber(5)
  MountRequest_MountType get mountType => $_getN(4);
  @$pb.TagNumber(5)
  set mountType(MountRequest_MountType v) { setField(5, v); }
  @$pb.TagNumber(5)
  $core.bool hasMountType() => $_has(4);
  @$pb.TagNumber(5)
  void clearMountType() => clearField(5);

  @$pb.TagNumber(6)
  UserCredentials get userCredentials => $_getN(5);
  @$pb.TagNumber(6)
  set userCredentials(UserCredentials v) { setField(6, v); }
  @$pb.TagNumber(6)
  $core.bool hasUserCredentials() => $_has(5);
  @$pb.TagNumber(6)
  void clearUserCredentials() => clearField(6);
  @$pb.TagNumber(6)
  UserCredentials ensureUserCredentials() => $_ensure(5);
}

class MountReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'MountReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'replyMessage')
    ..aOB(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'credentialsRequested')
    ..hasRequiredFields = false
  ;

  MountReply._() : super();
  factory MountReply({
    $core.String? logLine,
    $core.String? replyMessage,
    $core.bool? credentialsRequested,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (replyMessage != null) {
      _result.replyMessage = replyMessage;
    }
    if (credentialsRequested != null) {
      _result.credentialsRequested = credentialsRequested;
    }
    return _result;
  }
  factory MountReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory MountReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  MountReply clone() => MountReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  MountReply copyWith(void Function(MountReply) updates) => super.copyWith((message) => updates(message as MountReply)) as MountReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static MountReply create() => MountReply._();
  MountReply createEmptyInstance() => create();
  static $pb.PbList<MountReply> createRepeated() => $pb.PbList<MountReply>();
  @$core.pragma('dart2js:noInline')
  static MountReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<MountReply>(create);
  static MountReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get replyMessage => $_getSZ(1);
  @$pb.TagNumber(2)
  set replyMessage($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasReplyMessage() => $_has(1);
  @$pb.TagNumber(2)
  void clearReplyMessage() => clearField(2);

  @$pb.TagNumber(3)
  $core.bool get credentialsRequested => $_getBF(2);
  @$pb.TagNumber(3)
  set credentialsRequested($core.bool v) { $_setBool(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasCredentialsRequested() => $_has(2);
  @$pb.TagNumber(3)
  void clearCredentialsRequested() => clearField(3);
}

class PingRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'PingRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..hasRequiredFields = false
  ;

  PingRequest._() : super();
  factory PingRequest() => create();
  factory PingRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory PingRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  PingRequest clone() => PingRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  PingRequest copyWith(void Function(PingRequest) updates) => super.copyWith((message) => updates(message as PingRequest)) as PingRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static PingRequest create() => PingRequest._();
  PingRequest createEmptyInstance() => create();
  static $pb.PbList<PingRequest> createRepeated() => $pb.PbList<PingRequest>();
  @$core.pragma('dart2js:noInline')
  static PingRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<PingRequest>(create);
  static PingRequest? _defaultInstance;
}

class PingReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'PingReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..hasRequiredFields = false
  ;

  PingReply._() : super();
  factory PingReply() => create();
  factory PingReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory PingReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  PingReply clone() => PingReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  PingReply copyWith(void Function(PingReply) updates) => super.copyWith((message) => updates(message as PingReply)) as PingReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static PingReply create() => PingReply._();
  PingReply createEmptyInstance() => create();
  static $pb.PbList<PingReply> createRepeated() => $pb.PbList<PingReply>();
  @$core.pragma('dart2js:noInline')
  static PingReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<PingReply>(create);
  static PingReply? _defaultInstance;
}

class RecoverRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'RecoverRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  RecoverRequest._() : super();
  factory RecoverRequest({
    InstanceNames? instanceNames,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory RecoverRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory RecoverRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  RecoverRequest clone() => RecoverRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  RecoverRequest copyWith(void Function(RecoverRequest) updates) => super.copyWith((message) => updates(message as RecoverRequest)) as RecoverRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static RecoverRequest create() => RecoverRequest._();
  RecoverRequest createEmptyInstance() => create();
  static $pb.PbList<RecoverRequest> createRepeated() => $pb.PbList<RecoverRequest>();
  @$core.pragma('dart2js:noInline')
  static RecoverRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<RecoverRequest>(create);
  static RecoverRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);
}

class RecoverReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'RecoverReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  RecoverReply._() : super();
  factory RecoverReply({
    $core.String? logLine,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory RecoverReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory RecoverReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  RecoverReply clone() => RecoverReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  RecoverReply copyWith(void Function(RecoverReply) updates) => super.copyWith((message) => updates(message as RecoverReply)) as RecoverReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static RecoverReply create() => RecoverReply._();
  RecoverReply createEmptyInstance() => create();
  static $pb.PbList<RecoverReply> createRepeated() => $pb.PbList<RecoverReply>();
  @$core.pragma('dart2js:noInline')
  static RecoverReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<RecoverReply>(create);
  static RecoverReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);
}

class SSHInfoRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SSHInfoRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pPS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceName')
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  SSHInfoRequest._() : super();
  factory SSHInfoRequest({
    $core.Iterable<$core.String>? instanceName,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (instanceName != null) {
      _result.instanceName.addAll(instanceName);
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory SSHInfoRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SSHInfoRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SSHInfoRequest clone() => SSHInfoRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SSHInfoRequest copyWith(void Function(SSHInfoRequest) updates) => super.copyWith((message) => updates(message as SSHInfoRequest)) as SSHInfoRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SSHInfoRequest create() => SSHInfoRequest._();
  SSHInfoRequest createEmptyInstance() => create();
  static $pb.PbList<SSHInfoRequest> createRepeated() => $pb.PbList<SSHInfoRequest>();
  @$core.pragma('dart2js:noInline')
  static SSHInfoRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SSHInfoRequest>(create);
  static SSHInfoRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.String> get instanceName => $_getList(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);
}

class SSHInfo extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SSHInfo', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'port', $pb.PbFieldType.O3)
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'privKeyBase64')
    ..aOS(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'host')
    ..aOS(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'username')
    ..hasRequiredFields = false
  ;

  SSHInfo._() : super();
  factory SSHInfo({
    $core.int? port,
    $core.String? privKeyBase64,
    $core.String? host,
    $core.String? username,
  }) {
    final _result = create();
    if (port != null) {
      _result.port = port;
    }
    if (privKeyBase64 != null) {
      _result.privKeyBase64 = privKeyBase64;
    }
    if (host != null) {
      _result.host = host;
    }
    if (username != null) {
      _result.username = username;
    }
    return _result;
  }
  factory SSHInfo.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SSHInfo.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SSHInfo clone() => SSHInfo()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SSHInfo copyWith(void Function(SSHInfo) updates) => super.copyWith((message) => updates(message as SSHInfo)) as SSHInfo; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SSHInfo create() => SSHInfo._();
  SSHInfo createEmptyInstance() => create();
  static $pb.PbList<SSHInfo> createRepeated() => $pb.PbList<SSHInfo>();
  @$core.pragma('dart2js:noInline')
  static SSHInfo getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SSHInfo>(create);
  static SSHInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get port => $_getIZ(0);
  @$pb.TagNumber(1)
  set port($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasPort() => $_has(0);
  @$pb.TagNumber(1)
  void clearPort() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get privKeyBase64 => $_getSZ(1);
  @$pb.TagNumber(2)
  set privKeyBase64($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasPrivKeyBase64() => $_has(1);
  @$pb.TagNumber(2)
  void clearPrivKeyBase64() => clearField(2);

  @$pb.TagNumber(3)
  $core.String get host => $_getSZ(2);
  @$pb.TagNumber(3)
  set host($core.String v) { $_setString(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasHost() => $_has(2);
  @$pb.TagNumber(3)
  void clearHost() => clearField(3);

  @$pb.TagNumber(4)
  $core.String get username => $_getSZ(3);
  @$pb.TagNumber(4)
  set username($core.String v) { $_setString(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasUsername() => $_has(3);
  @$pb.TagNumber(4)
  void clearUsername() => clearField(4);
}

class SSHInfoReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SSHInfoReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..m<$core.String, SSHInfo>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'sshInfo', entryClassName: 'SSHInfoReply.SshInfoEntry', keyFieldType: $pb.PbFieldType.OS, valueFieldType: $pb.PbFieldType.OM, valueCreator: SSHInfo.create, packageName: const $pb.PackageName('multipass'))
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  SSHInfoReply._() : super();
  factory SSHInfoReply({
    $core.Map<$core.String, SSHInfo>? sshInfo,
    $core.String? logLine,
  }) {
    final _result = create();
    if (sshInfo != null) {
      _result.sshInfo.addAll(sshInfo);
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory SSHInfoReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SSHInfoReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SSHInfoReply clone() => SSHInfoReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SSHInfoReply copyWith(void Function(SSHInfoReply) updates) => super.copyWith((message) => updates(message as SSHInfoReply)) as SSHInfoReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SSHInfoReply create() => SSHInfoReply._();
  SSHInfoReply createEmptyInstance() => create();
  static $pb.PbList<SSHInfoReply> createRepeated() => $pb.PbList<SSHInfoReply>();
  @$core.pragma('dart2js:noInline')
  static SSHInfoReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SSHInfoReply>(create);
  static SSHInfoReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.Map<$core.String, SSHInfo> get sshInfo => $_getMap(0);

  @$pb.TagNumber(2)
  $core.String get logLine => $_getSZ(1);
  @$pb.TagNumber(2)
  set logLine($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasLogLine() => $_has(1);
  @$pb.TagNumber(2)
  void clearLogLine() => clearField(2);
}

class StartError extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'StartError', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..m<$core.String, StartError_ErrorCode>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceErrors', entryClassName: 'StartError.InstanceErrorsEntry', keyFieldType: $pb.PbFieldType.OS, valueFieldType: $pb.PbFieldType.OE, valueOf: StartError_ErrorCode.valueOf, enumValues: StartError_ErrorCode.values, defaultEnumValue: StartError_ErrorCode.OK, packageName: const $pb.PackageName('multipass'))
    ..hasRequiredFields = false
  ;

  StartError._() : super();
  factory StartError({
    $core.Map<$core.String, StartError_ErrorCode>? instanceErrors,
  }) {
    final _result = create();
    if (instanceErrors != null) {
      _result.instanceErrors.addAll(instanceErrors);
    }
    return _result;
  }
  factory StartError.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory StartError.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  StartError clone() => StartError()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  StartError copyWith(void Function(StartError) updates) => super.copyWith((message) => updates(message as StartError)) as StartError; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static StartError create() => StartError._();
  StartError createEmptyInstance() => create();
  static $pb.PbList<StartError> createRepeated() => $pb.PbList<StartError>();
  @$core.pragma('dart2js:noInline')
  static StartError getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<StartError>(create);
  static StartError? _defaultInstance;

  @$pb.TagNumber(1)
  $core.Map<$core.String, StartError_ErrorCode> get instanceErrors => $_getMap(0);
}

class StartRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'StartRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..a<$core.int>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'timeout', $pb.PbFieldType.O3)
    ..aOM<UserCredentials>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'userCredentials', subBuilder: UserCredentials.create)
    ..hasRequiredFields = false
  ;

  StartRequest._() : super();
  factory StartRequest({
    InstanceNames? instanceNames,
    $core.int? verbosityLevel,
    $core.int? timeout,
    UserCredentials? userCredentials,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (timeout != null) {
      _result.timeout = timeout;
    }
    if (userCredentials != null) {
      _result.userCredentials = userCredentials;
    }
    return _result;
  }
  factory StartRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory StartRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  StartRequest clone() => StartRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  StartRequest copyWith(void Function(StartRequest) updates) => super.copyWith((message) => updates(message as StartRequest)) as StartRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static StartRequest create() => StartRequest._();
  StartRequest createEmptyInstance() => create();
  static $pb.PbList<StartRequest> createRepeated() => $pb.PbList<StartRequest>();
  @$core.pragma('dart2js:noInline')
  static StartRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<StartRequest>(create);
  static StartRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);

  @$pb.TagNumber(3)
  $core.int get timeout => $_getIZ(2);
  @$pb.TagNumber(3)
  set timeout($core.int v) { $_setSignedInt32(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasTimeout() => $_has(2);
  @$pb.TagNumber(3)
  void clearTimeout() => clearField(3);

  @$pb.TagNumber(4)
  UserCredentials get userCredentials => $_getN(3);
  @$pb.TagNumber(4)
  set userCredentials(UserCredentials v) { setField(4, v); }
  @$pb.TagNumber(4)
  $core.bool hasUserCredentials() => $_has(3);
  @$pb.TagNumber(4)
  void clearUserCredentials() => clearField(4);
  @$pb.TagNumber(4)
  UserCredentials ensureUserCredentials() => $_ensure(3);
}

class StartReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'StartReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'replyMessage')
    ..aOM<UpdateInfo>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'updateInfo', subBuilder: UpdateInfo.create)
    ..aOB(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'credentialsRequested')
    ..hasRequiredFields = false
  ;

  StartReply._() : super();
  factory StartReply({
    $core.String? logLine,
    $core.String? replyMessage,
    UpdateInfo? updateInfo,
    $core.bool? credentialsRequested,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (replyMessage != null) {
      _result.replyMessage = replyMessage;
    }
    if (updateInfo != null) {
      _result.updateInfo = updateInfo;
    }
    if (credentialsRequested != null) {
      _result.credentialsRequested = credentialsRequested;
    }
    return _result;
  }
  factory StartReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory StartReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  StartReply clone() => StartReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  StartReply copyWith(void Function(StartReply) updates) => super.copyWith((message) => updates(message as StartReply)) as StartReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static StartReply create() => StartReply._();
  StartReply createEmptyInstance() => create();
  static $pb.PbList<StartReply> createRepeated() => $pb.PbList<StartReply>();
  @$core.pragma('dart2js:noInline')
  static StartReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<StartReply>(create);
  static StartReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get replyMessage => $_getSZ(1);
  @$pb.TagNumber(2)
  set replyMessage($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasReplyMessage() => $_has(1);
  @$pb.TagNumber(2)
  void clearReplyMessage() => clearField(2);

  @$pb.TagNumber(3)
  UpdateInfo get updateInfo => $_getN(2);
  @$pb.TagNumber(3)
  set updateInfo(UpdateInfo v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasUpdateInfo() => $_has(2);
  @$pb.TagNumber(3)
  void clearUpdateInfo() => clearField(3);
  @$pb.TagNumber(3)
  UpdateInfo ensureUpdateInfo() => $_ensure(2);

  @$pb.TagNumber(4)
  $core.bool get credentialsRequested => $_getBF(3);
  @$pb.TagNumber(4)
  set credentialsRequested($core.bool v) { $_setBool(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasCredentialsRequested() => $_has(3);
  @$pb.TagNumber(4)
  void clearCredentialsRequested() => clearField(4);
}

class StopRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'StopRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'timeMinutes', $pb.PbFieldType.O3)
    ..aOB(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'cancelShutdown')
    ..a<$core.int>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  StopRequest._() : super();
  factory StopRequest({
    InstanceNames? instanceNames,
    $core.int? timeMinutes,
    $core.bool? cancelShutdown,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (timeMinutes != null) {
      _result.timeMinutes = timeMinutes;
    }
    if (cancelShutdown != null) {
      _result.cancelShutdown = cancelShutdown;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory StopRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory StopRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  StopRequest clone() => StopRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  StopRequest copyWith(void Function(StopRequest) updates) => super.copyWith((message) => updates(message as StopRequest)) as StopRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static StopRequest create() => StopRequest._();
  StopRequest createEmptyInstance() => create();
  static $pb.PbList<StopRequest> createRepeated() => $pb.PbList<StopRequest>();
  @$core.pragma('dart2js:noInline')
  static StopRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<StopRequest>(create);
  static StopRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.int get timeMinutes => $_getIZ(1);
  @$pb.TagNumber(2)
  set timeMinutes($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasTimeMinutes() => $_has(1);
  @$pb.TagNumber(2)
  void clearTimeMinutes() => clearField(2);

  @$pb.TagNumber(3)
  $core.bool get cancelShutdown => $_getBF(2);
  @$pb.TagNumber(3)
  set cancelShutdown($core.bool v) { $_setBool(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasCancelShutdown() => $_has(2);
  @$pb.TagNumber(3)
  void clearCancelShutdown() => clearField(3);

  @$pb.TagNumber(4)
  $core.int get verbosityLevel => $_getIZ(3);
  @$pb.TagNumber(4)
  set verbosityLevel($core.int v) { $_setSignedInt32(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasVerbosityLevel() => $_has(3);
  @$pb.TagNumber(4)
  void clearVerbosityLevel() => clearField(4);
}

class StopReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'StopReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  StopReply._() : super();
  factory StopReply({
    $core.String? logLine,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory StopReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory StopReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  StopReply clone() => StopReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  StopReply copyWith(void Function(StopReply) updates) => super.copyWith((message) => updates(message as StopReply)) as StopReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static StopReply create() => StopReply._();
  StopReply createEmptyInstance() => create();
  static $pb.PbList<StopReply> createRepeated() => $pb.PbList<StopReply>();
  @$core.pragma('dart2js:noInline')
  static StopReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<StopReply>(create);
  static StopReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);
}

class SuspendRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SuspendRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  SuspendRequest._() : super();
  factory SuspendRequest({
    InstanceNames? instanceNames,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory SuspendRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SuspendRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SuspendRequest clone() => SuspendRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SuspendRequest copyWith(void Function(SuspendRequest) updates) => super.copyWith((message) => updates(message as SuspendRequest)) as SuspendRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SuspendRequest create() => SuspendRequest._();
  SuspendRequest createEmptyInstance() => create();
  static $pb.PbList<SuspendRequest> createRepeated() => $pb.PbList<SuspendRequest>();
  @$core.pragma('dart2js:noInline')
  static SuspendRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SuspendRequest>(create);
  static SuspendRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);
}

class SuspendReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SuspendReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  SuspendReply._() : super();
  factory SuspendReply({
    $core.String? logLine,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory SuspendReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SuspendReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SuspendReply clone() => SuspendReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SuspendReply copyWith(void Function(SuspendReply) updates) => super.copyWith((message) => updates(message as SuspendReply)) as SuspendReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SuspendReply create() => SuspendReply._();
  SuspendReply createEmptyInstance() => create();
  static $pb.PbList<SuspendReply> createRepeated() => $pb.PbList<SuspendReply>();
  @$core.pragma('dart2js:noInline')
  static SuspendReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SuspendReply>(create);
  static SuspendReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);
}

class RestartRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'RestartRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..a<$core.int>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'timeout', $pb.PbFieldType.O3)
    ..aOM<UserCredentials>(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'userCredentials', subBuilder: UserCredentials.create)
    ..hasRequiredFields = false
  ;

  RestartRequest._() : super();
  factory RestartRequest({
    InstanceNames? instanceNames,
    $core.int? verbosityLevel,
    $core.int? timeout,
    UserCredentials? userCredentials,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    if (timeout != null) {
      _result.timeout = timeout;
    }
    if (userCredentials != null) {
      _result.userCredentials = userCredentials;
    }
    return _result;
  }
  factory RestartRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory RestartRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  RestartRequest clone() => RestartRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  RestartRequest copyWith(void Function(RestartRequest) updates) => super.copyWith((message) => updates(message as RestartRequest)) as RestartRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static RestartRequest create() => RestartRequest._();
  RestartRequest createEmptyInstance() => create();
  static $pb.PbList<RestartRequest> createRepeated() => $pb.PbList<RestartRequest>();
  @$core.pragma('dart2js:noInline')
  static RestartRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<RestartRequest>(create);
  static RestartRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);

  @$pb.TagNumber(3)
  $core.int get timeout => $_getIZ(2);
  @$pb.TagNumber(3)
  set timeout($core.int v) { $_setSignedInt32(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasTimeout() => $_has(2);
  @$pb.TagNumber(3)
  void clearTimeout() => clearField(3);

  @$pb.TagNumber(4)
  UserCredentials get userCredentials => $_getN(3);
  @$pb.TagNumber(4)
  set userCredentials(UserCredentials v) { setField(4, v); }
  @$pb.TagNumber(4)
  $core.bool hasUserCredentials() => $_has(3);
  @$pb.TagNumber(4)
  void clearUserCredentials() => clearField(4);
  @$pb.TagNumber(4)
  UserCredentials ensureUserCredentials() => $_ensure(3);
}

class RestartReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'RestartReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'replyMessage')
    ..aOM<UpdateInfo>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'updateInfo', subBuilder: UpdateInfo.create)
    ..aOB(4, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'credentialsRequested')
    ..hasRequiredFields = false
  ;

  RestartReply._() : super();
  factory RestartReply({
    $core.String? logLine,
    $core.String? replyMessage,
    UpdateInfo? updateInfo,
    $core.bool? credentialsRequested,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (replyMessage != null) {
      _result.replyMessage = replyMessage;
    }
    if (updateInfo != null) {
      _result.updateInfo = updateInfo;
    }
    if (credentialsRequested != null) {
      _result.credentialsRequested = credentialsRequested;
    }
    return _result;
  }
  factory RestartReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory RestartReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  RestartReply clone() => RestartReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  RestartReply copyWith(void Function(RestartReply) updates) => super.copyWith((message) => updates(message as RestartReply)) as RestartReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static RestartReply create() => RestartReply._();
  RestartReply createEmptyInstance() => create();
  static $pb.PbList<RestartReply> createRepeated() => $pb.PbList<RestartReply>();
  @$core.pragma('dart2js:noInline')
  static RestartReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<RestartReply>(create);
  static RestartReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get replyMessage => $_getSZ(1);
  @$pb.TagNumber(2)
  set replyMessage($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasReplyMessage() => $_has(1);
  @$pb.TagNumber(2)
  void clearReplyMessage() => clearField(2);

  @$pb.TagNumber(3)
  UpdateInfo get updateInfo => $_getN(2);
  @$pb.TagNumber(3)
  set updateInfo(UpdateInfo v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasUpdateInfo() => $_has(2);
  @$pb.TagNumber(3)
  void clearUpdateInfo() => clearField(3);
  @$pb.TagNumber(3)
  UpdateInfo ensureUpdateInfo() => $_ensure(2);

  @$pb.TagNumber(4)
  $core.bool get credentialsRequested => $_getBF(3);
  @$pb.TagNumber(4)
  set credentialsRequested($core.bool v) { $_setBool(3, v); }
  @$pb.TagNumber(4)
  $core.bool hasCredentialsRequested() => $_has(3);
  @$pb.TagNumber(4)
  void clearCredentialsRequested() => clearField(4);
}

class DeleteRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'DeleteRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOM<InstanceNames>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'instanceNames', subBuilder: InstanceNames.create)
    ..aOB(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'purge')
    ..a<$core.int>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  DeleteRequest._() : super();
  factory DeleteRequest({
    InstanceNames? instanceNames,
    $core.bool? purge,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (instanceNames != null) {
      _result.instanceNames = instanceNames;
    }
    if (purge != null) {
      _result.purge = purge;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory DeleteRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory DeleteRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  DeleteRequest clone() => DeleteRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  DeleteRequest copyWith(void Function(DeleteRequest) updates) => super.copyWith((message) => updates(message as DeleteRequest)) as DeleteRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static DeleteRequest create() => DeleteRequest._();
  DeleteRequest createEmptyInstance() => create();
  static $pb.PbList<DeleteRequest> createRepeated() => $pb.PbList<DeleteRequest>();
  @$core.pragma('dart2js:noInline')
  static DeleteRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<DeleteRequest>(create);
  static DeleteRequest? _defaultInstance;

  @$pb.TagNumber(1)
  InstanceNames get instanceNames => $_getN(0);
  @$pb.TagNumber(1)
  set instanceNames(InstanceNames v) { setField(1, v); }
  @$pb.TagNumber(1)
  $core.bool hasInstanceNames() => $_has(0);
  @$pb.TagNumber(1)
  void clearInstanceNames() => clearField(1);
  @$pb.TagNumber(1)
  InstanceNames ensureInstanceNames() => $_ensure(0);

  @$pb.TagNumber(2)
  $core.bool get purge => $_getBF(1);
  @$pb.TagNumber(2)
  set purge($core.bool v) { $_setBool(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasPurge() => $_has(1);
  @$pb.TagNumber(2)
  void clearPurge() => clearField(2);

  @$pb.TagNumber(3)
  $core.int get verbosityLevel => $_getIZ(2);
  @$pb.TagNumber(3)
  set verbosityLevel($core.int v) { $_setSignedInt32(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasVerbosityLevel() => $_has(2);
  @$pb.TagNumber(3)
  void clearVerbosityLevel() => clearField(3);
}

class DeleteReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'DeleteReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..pPS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'purgedInstances')
    ..hasRequiredFields = false
  ;

  DeleteReply._() : super();
  factory DeleteReply({
    $core.String? logLine,
    $core.Iterable<$core.String>? purgedInstances,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (purgedInstances != null) {
      _result.purgedInstances.addAll(purgedInstances);
    }
    return _result;
  }
  factory DeleteReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory DeleteReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  DeleteReply clone() => DeleteReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  DeleteReply copyWith(void Function(DeleteReply) updates) => super.copyWith((message) => updates(message as DeleteReply)) as DeleteReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static DeleteReply create() => DeleteReply._();
  DeleteReply createEmptyInstance() => create();
  static $pb.PbList<DeleteReply> createRepeated() => $pb.PbList<DeleteReply>();
  @$core.pragma('dart2js:noInline')
  static DeleteReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<DeleteReply>(create);
  static DeleteReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.String> get purgedInstances => $_getList(1);
}

class UmountRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'UmountRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..pc<TargetPathInfo>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'targetPaths', $pb.PbFieldType.PM, subBuilder: TargetPathInfo.create)
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  UmountRequest._() : super();
  factory UmountRequest({
    $core.Iterable<TargetPathInfo>? targetPaths,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (targetPaths != null) {
      _result.targetPaths.addAll(targetPaths);
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory UmountRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory UmountRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  UmountRequest clone() => UmountRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  UmountRequest copyWith(void Function(UmountRequest) updates) => super.copyWith((message) => updates(message as UmountRequest)) as UmountRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static UmountRequest create() => UmountRequest._();
  UmountRequest createEmptyInstance() => create();
  static $pb.PbList<UmountRequest> createRepeated() => $pb.PbList<UmountRequest>();
  @$core.pragma('dart2js:noInline')
  static UmountRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<UmountRequest>(create);
  static UmountRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<TargetPathInfo> get targetPaths => $_getList(0);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);
}

class UmountReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'UmountReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  UmountReply._() : super();
  factory UmountReply({
    $core.String? logLine,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory UmountReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory UmountReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  UmountReply clone() => UmountReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  UmountReply copyWith(void Function(UmountReply) updates) => super.copyWith((message) => updates(message as UmountReply)) as UmountReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static UmountReply create() => UmountReply._();
  UmountReply createEmptyInstance() => create();
  static $pb.PbList<UmountReply> createRepeated() => $pb.PbList<UmountReply>();
  @$core.pragma('dart2js:noInline')
  static UmountReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<UmountReply>(create);
  static UmountReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);
}

class VersionRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'VersionRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  VersionRequest._() : super();
  factory VersionRequest({
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory VersionRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory VersionRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  VersionRequest clone() => VersionRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  VersionRequest copyWith(void Function(VersionRequest) updates) => super.copyWith((message) => updates(message as VersionRequest)) as VersionRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static VersionRequest create() => VersionRequest._();
  VersionRequest createEmptyInstance() => create();
  static $pb.PbList<VersionRequest> createRepeated() => $pb.PbList<VersionRequest>();
  @$core.pragma('dart2js:noInline')
  static VersionRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<VersionRequest>(create);
  static VersionRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get verbosityLevel => $_getIZ(0);
  @$pb.TagNumber(1)
  set verbosityLevel($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVerbosityLevel() => $_has(0);
  @$pb.TagNumber(1)
  void clearVerbosityLevel() => clearField(1);
}

class VersionReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'VersionReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'version')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOM<UpdateInfo>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'updateInfo', subBuilder: UpdateInfo.create)
    ..hasRequiredFields = false
  ;

  VersionReply._() : super();
  factory VersionReply({
    $core.String? version,
    $core.String? logLine,
    UpdateInfo? updateInfo,
  }) {
    final _result = create();
    if (version != null) {
      _result.version = version;
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (updateInfo != null) {
      _result.updateInfo = updateInfo;
    }
    return _result;
  }
  factory VersionReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory VersionReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  VersionReply clone() => VersionReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  VersionReply copyWith(void Function(VersionReply) updates) => super.copyWith((message) => updates(message as VersionReply)) as VersionReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static VersionReply create() => VersionReply._();
  VersionReply createEmptyInstance() => create();
  static $pb.PbList<VersionReply> createRepeated() => $pb.PbList<VersionReply>();
  @$core.pragma('dart2js:noInline')
  static VersionReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<VersionReply>(create);
  static VersionReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get version => $_getSZ(0);
  @$pb.TagNumber(1)
  set version($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVersion() => $_has(0);
  @$pb.TagNumber(1)
  void clearVersion() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get logLine => $_getSZ(1);
  @$pb.TagNumber(2)
  set logLine($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasLogLine() => $_has(1);
  @$pb.TagNumber(2)
  void clearLogLine() => clearField(2);

  @$pb.TagNumber(3)
  UpdateInfo get updateInfo => $_getN(2);
  @$pb.TagNumber(3)
  set updateInfo(UpdateInfo v) { setField(3, v); }
  @$pb.TagNumber(3)
  $core.bool hasUpdateInfo() => $_has(2);
  @$pb.TagNumber(3)
  void clearUpdateInfo() => clearField(3);
  @$pb.TagNumber(3)
  UpdateInfo ensureUpdateInfo() => $_ensure(2);
}

class GetRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'GetRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'key')
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  GetRequest._() : super();
  factory GetRequest({
    $core.String? key,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (key != null) {
      _result.key = key;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory GetRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory GetRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  GetRequest clone() => GetRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  GetRequest copyWith(void Function(GetRequest) updates) => super.copyWith((message) => updates(message as GetRequest)) as GetRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static GetRequest create() => GetRequest._();
  GetRequest createEmptyInstance() => create();
  static $pb.PbList<GetRequest> createRepeated() => $pb.PbList<GetRequest>();
  @$core.pragma('dart2js:noInline')
  static GetRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<GetRequest>(create);
  static GetRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get key => $_getSZ(0);
  @$pb.TagNumber(1)
  set key($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasKey() => $_has(0);
  @$pb.TagNumber(1)
  void clearKey() => clearField(1);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);
}

class GetReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'GetReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'value')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  GetReply._() : super();
  factory GetReply({
    $core.String? value,
    $core.String? logLine,
  }) {
    final _result = create();
    if (value != null) {
      _result.value = value;
    }
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory GetReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory GetReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  GetReply clone() => GetReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  GetReply copyWith(void Function(GetReply) updates) => super.copyWith((message) => updates(message as GetReply)) as GetReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static GetReply create() => GetReply._();
  GetReply createEmptyInstance() => create();
  static $pb.PbList<GetReply> createRepeated() => $pb.PbList<GetReply>();
  @$core.pragma('dart2js:noInline')
  static GetReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<GetReply>(create);
  static GetReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get value => $_getSZ(0);
  @$pb.TagNumber(1)
  set value($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasValue() => $_has(0);
  @$pb.TagNumber(1)
  void clearValue() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get logLine => $_getSZ(1);
  @$pb.TagNumber(2)
  set logLine($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasLogLine() => $_has(1);
  @$pb.TagNumber(2)
  void clearLogLine() => clearField(2);
}

class SetRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SetRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'key')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'val')
    ..a<$core.int>(3, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  SetRequest._() : super();
  factory SetRequest({
    $core.String? key,
    $core.String? val,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (key != null) {
      _result.key = key;
    }
    if (val != null) {
      _result.val = val;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory SetRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SetRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SetRequest clone() => SetRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SetRequest copyWith(void Function(SetRequest) updates) => super.copyWith((message) => updates(message as SetRequest)) as SetRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SetRequest create() => SetRequest._();
  SetRequest createEmptyInstance() => create();
  static $pb.PbList<SetRequest> createRepeated() => $pb.PbList<SetRequest>();
  @$core.pragma('dart2js:noInline')
  static SetRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SetRequest>(create);
  static SetRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get key => $_getSZ(0);
  @$pb.TagNumber(1)
  set key($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasKey() => $_has(0);
  @$pb.TagNumber(1)
  void clearKey() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get val => $_getSZ(1);
  @$pb.TagNumber(2)
  set val($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVal() => $_has(1);
  @$pb.TagNumber(2)
  void clearVal() => clearField(2);

  @$pb.TagNumber(3)
  $core.int get verbosityLevel => $_getIZ(2);
  @$pb.TagNumber(3)
  set verbosityLevel($core.int v) { $_setSignedInt32(2, v); }
  @$pb.TagNumber(3)
  $core.bool hasVerbosityLevel() => $_has(2);
  @$pb.TagNumber(3)
  void clearVerbosityLevel() => clearField(3);
}

class SetReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'SetReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..aOS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'replyMessage')
    ..hasRequiredFields = false
  ;

  SetReply._() : super();
  factory SetReply({
    $core.String? logLine,
    $core.String? replyMessage,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (replyMessage != null) {
      _result.replyMessage = replyMessage;
    }
    return _result;
  }
  factory SetReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory SetReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  SetReply clone() => SetReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  SetReply copyWith(void Function(SetReply) updates) => super.copyWith((message) => updates(message as SetReply)) as SetReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static SetReply create() => SetReply._();
  SetReply createEmptyInstance() => create();
  static $pb.PbList<SetReply> createRepeated() => $pb.PbList<SetReply>();
  @$core.pragma('dart2js:noInline')
  static SetReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<SetReply>(create);
  static SetReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.String get replyMessage => $_getSZ(1);
  @$pb.TagNumber(2)
  set replyMessage($core.String v) { $_setString(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasReplyMessage() => $_has(1);
  @$pb.TagNumber(2)
  void clearReplyMessage() => clearField(2);
}

class KeysRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'KeysRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..a<$core.int>(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  KeysRequest._() : super();
  factory KeysRequest({
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory KeysRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory KeysRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  KeysRequest clone() => KeysRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  KeysRequest copyWith(void Function(KeysRequest) updates) => super.copyWith((message) => updates(message as KeysRequest)) as KeysRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static KeysRequest create() => KeysRequest._();
  KeysRequest createEmptyInstance() => create();
  static $pb.PbList<KeysRequest> createRepeated() => $pb.PbList<KeysRequest>();
  @$core.pragma('dart2js:noInline')
  static KeysRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<KeysRequest>(create);
  static KeysRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.int get verbosityLevel => $_getIZ(0);
  @$pb.TagNumber(1)
  set verbosityLevel($core.int v) { $_setSignedInt32(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasVerbosityLevel() => $_has(0);
  @$pb.TagNumber(1)
  void clearVerbosityLevel() => clearField(1);
}

class KeysReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'KeysReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..pPS(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'settingsKeys')
    ..hasRequiredFields = false
  ;

  KeysReply._() : super();
  factory KeysReply({
    $core.String? logLine,
    $core.Iterable<$core.String>? settingsKeys,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    if (settingsKeys != null) {
      _result.settingsKeys.addAll(settingsKeys);
    }
    return _result;
  }
  factory KeysReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory KeysReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  KeysReply clone() => KeysReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  KeysReply copyWith(void Function(KeysReply) updates) => super.copyWith((message) => updates(message as KeysReply)) as KeysReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static KeysReply create() => KeysReply._();
  KeysReply createEmptyInstance() => create();
  static $pb.PbList<KeysReply> createRepeated() => $pb.PbList<KeysReply>();
  @$core.pragma('dart2js:noInline')
  static KeysReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<KeysReply>(create);
  static KeysReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.String> get settingsKeys => $_getList(1);
}

class AuthenticateRequest extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'AuthenticateRequest', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'passphrase')
    ..a<$core.int>(2, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'verbosityLevel', $pb.PbFieldType.O3)
    ..hasRequiredFields = false
  ;

  AuthenticateRequest._() : super();
  factory AuthenticateRequest({
    $core.String? passphrase,
    $core.int? verbosityLevel,
  }) {
    final _result = create();
    if (passphrase != null) {
      _result.passphrase = passphrase;
    }
    if (verbosityLevel != null) {
      _result.verbosityLevel = verbosityLevel;
    }
    return _result;
  }
  factory AuthenticateRequest.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory AuthenticateRequest.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  AuthenticateRequest clone() => AuthenticateRequest()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  AuthenticateRequest copyWith(void Function(AuthenticateRequest) updates) => super.copyWith((message) => updates(message as AuthenticateRequest)) as AuthenticateRequest; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static AuthenticateRequest create() => AuthenticateRequest._();
  AuthenticateRequest createEmptyInstance() => create();
  static $pb.PbList<AuthenticateRequest> createRepeated() => $pb.PbList<AuthenticateRequest>();
  @$core.pragma('dart2js:noInline')
  static AuthenticateRequest getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<AuthenticateRequest>(create);
  static AuthenticateRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get passphrase => $_getSZ(0);
  @$pb.TagNumber(1)
  set passphrase($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasPassphrase() => $_has(0);
  @$pb.TagNumber(1)
  void clearPassphrase() => clearField(1);

  @$pb.TagNumber(2)
  $core.int get verbosityLevel => $_getIZ(1);
  @$pb.TagNumber(2)
  set verbosityLevel($core.int v) { $_setSignedInt32(1, v); }
  @$pb.TagNumber(2)
  $core.bool hasVerbosityLevel() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerbosityLevel() => clearField(2);
}

class AuthenticateReply extends $pb.GeneratedMessage {
  static final $pb.BuilderInfo _i = $pb.BuilderInfo(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'AuthenticateReply', package: const $pb.PackageName(const $core.bool.fromEnvironment('protobuf.omit_message_names') ? '' : 'multipass'), createEmptyInstance: create)
    ..aOS(1, const $core.bool.fromEnvironment('protobuf.omit_field_names') ? '' : 'logLine')
    ..hasRequiredFields = false
  ;

  AuthenticateReply._() : super();
  factory AuthenticateReply({
    $core.String? logLine,
  }) {
    final _result = create();
    if (logLine != null) {
      _result.logLine = logLine;
    }
    return _result;
  }
  factory AuthenticateReply.fromBuffer($core.List<$core.int> i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromBuffer(i, r);
  factory AuthenticateReply.fromJson($core.String i, [$pb.ExtensionRegistry r = $pb.ExtensionRegistry.EMPTY]) => create()..mergeFromJson(i, r);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.deepCopy] instead. '
  'Will be removed in next major version')
  AuthenticateReply clone() => AuthenticateReply()..mergeFromMessage(this);
  @$core.Deprecated(
  'Using this can add significant overhead to your binary. '
  'Use [GeneratedMessageGenericExtensions.rebuild] instead. '
  'Will be removed in next major version')
  AuthenticateReply copyWith(void Function(AuthenticateReply) updates) => super.copyWith((message) => updates(message as AuthenticateReply)) as AuthenticateReply; // ignore: deprecated_member_use
  $pb.BuilderInfo get info_ => _i;
  @$core.pragma('dart2js:noInline')
  static AuthenticateReply create() => AuthenticateReply._();
  AuthenticateReply createEmptyInstance() => create();
  static $pb.PbList<AuthenticateReply> createRepeated() => $pb.PbList<AuthenticateReply>();
  @$core.pragma('dart2js:noInline')
  static AuthenticateReply getDefault() => _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<AuthenticateReply>(create);
  static AuthenticateReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get logLine => $_getSZ(0);
  @$pb.TagNumber(1)
  set logLine($core.String v) { $_setString(0, v); }
  @$pb.TagNumber(1)
  $core.bool hasLogLine() => $_has(0);
  @$pb.TagNumber(1)
  void clearLogLine() => clearField(1);
}

