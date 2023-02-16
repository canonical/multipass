///
//  Generated code. Do not modify.
//  source: multipass.proto
//
// @dart = 2.12
// ignore_for_file: annotate_overrides,camel_case_types,constant_identifier_names,deprecated_member_use_from_same_package,directives_ordering,library_prefixes,non_constant_identifier_names,prefer_final_fields,return_of_invalid_type,unnecessary_const,unnecessary_import,unnecessary_this,unused_import,unused_shown_name

import 'dart:core' as $core;
import 'dart:convert' as $convert;
import 'dart:typed_data' as $typed_data;
@$core.Deprecated('Use launchRequestDescriptor instead')
const LaunchRequest$json = const {
  '1': 'LaunchRequest',
  '2': const [
    const {'1': 'instance_name', '3': 1, '4': 1, '5': 9, '10': 'instanceName'},
    const {'1': 'image', '3': 2, '4': 1, '5': 9, '10': 'image'},
    const {'1': 'kernel_name', '3': 3, '4': 1, '5': 9, '10': 'kernelName'},
    const {'1': 'num_cores', '3': 4, '4': 1, '5': 5, '10': 'numCores'},
    const {'1': 'mem_size', '3': 5, '4': 1, '5': 9, '10': 'memSize'},
    const {'1': 'disk_space', '3': 6, '4': 1, '5': 9, '10': 'diskSpace'},
    const {'1': 'time_zone', '3': 7, '4': 1, '5': 9, '10': 'timeZone'},
    const {'1': 'cloud_init_user_data', '3': 8, '4': 1, '5': 9, '10': 'cloudInitUserData'},
    const {'1': 'remote_name', '3': 9, '4': 1, '5': 9, '10': 'remoteName'},
    const {'1': 'verbosity_level', '3': 11, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'network_options', '3': 12, '4': 3, '5': 11, '6': '.multipass.LaunchRequest.NetworkOptions', '10': 'networkOptions'},
    const {'1': 'permission_to_bridge', '3': 13, '4': 1, '5': 8, '10': 'permissionToBridge'},
    const {'1': 'timeout', '3': 14, '4': 1, '5': 5, '10': 'timeout'},
    const {'1': 'user_credentials', '3': 15, '4': 1, '5': 11, '6': '.multipass.UserCredentials', '10': 'userCredentials'},
  ],
  '3': const [LaunchRequest_NetworkOptions$json],
};

@$core.Deprecated('Use launchRequestDescriptor instead')
const LaunchRequest_NetworkOptions$json = const {
  '1': 'NetworkOptions',
  '2': const [
    const {'1': 'id', '3': 1, '4': 1, '5': 9, '10': 'id'},
    const {'1': 'mode', '3': 2, '4': 1, '5': 14, '6': '.multipass.LaunchRequest.NetworkOptions.Mode', '10': 'mode'},
    const {'1': 'mac_address', '3': 3, '4': 1, '5': 9, '10': 'macAddress'},
  ],
  '4': const [LaunchRequest_NetworkOptions_Mode$json],
};

@$core.Deprecated('Use launchRequestDescriptor instead')
const LaunchRequest_NetworkOptions_Mode$json = const {
  '1': 'Mode',
  '2': const [
    const {'1': 'AUTO', '2': 0},
    const {'1': 'MANUAL', '2': 1},
  ],
};

/// Descriptor for `LaunchRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List launchRequestDescriptor = $convert.base64Decode('Cg1MYXVuY2hSZXF1ZXN0EiMKDWluc3RhbmNlX25hbWUYASABKAlSDGluc3RhbmNlTmFtZRIUCgVpbWFnZRgCIAEoCVIFaW1hZ2USHwoLa2VybmVsX25hbWUYAyABKAlSCmtlcm5lbE5hbWUSGwoJbnVtX2NvcmVzGAQgASgFUghudW1Db3JlcxIZCghtZW1fc2l6ZRgFIAEoCVIHbWVtU2l6ZRIdCgpkaXNrX3NwYWNlGAYgASgJUglkaXNrU3BhY2USGwoJdGltZV96b25lGAcgASgJUgh0aW1lWm9uZRIvChRjbG91ZF9pbml0X3VzZXJfZGF0YRgIIAEoCVIRY2xvdWRJbml0VXNlckRhdGESHwoLcmVtb3RlX25hbWUYCSABKAlSCnJlbW90ZU5hbWUSJwoPdmVyYm9zaXR5X2xldmVsGAsgASgFUg52ZXJib3NpdHlMZXZlbBJQCg9uZXR3b3JrX29wdGlvbnMYDCADKAsyJy5tdWx0aXBhc3MuTGF1bmNoUmVxdWVzdC5OZXR3b3JrT3B0aW9uc1IObmV0d29ya09wdGlvbnMSMAoUcGVybWlzc2lvbl90b19icmlkZ2UYDSABKAhSEnBlcm1pc3Npb25Ub0JyaWRnZRIYCgd0aW1lb3V0GA4gASgFUgd0aW1lb3V0EkUKEHVzZXJfY3JlZGVudGlhbHMYDyABKAsyGi5tdWx0aXBhc3MuVXNlckNyZWRlbnRpYWxzUg91c2VyQ3JlZGVudGlhbHMaoQEKDk5ldHdvcmtPcHRpb25zEg4KAmlkGAEgASgJUgJpZBJACgRtb2RlGAIgASgOMiwubXVsdGlwYXNzLkxhdW5jaFJlcXVlc3QuTmV0d29ya09wdGlvbnMuTW9kZVIEbW9kZRIfCgttYWNfYWRkcmVzcxgDIAEoCVIKbWFjQWRkcmVzcyIcCgRNb2RlEggKBEFVVE8QABIKCgZNQU5VQUwQAQ==');
@$core.Deprecated('Use launchErrorDescriptor instead')
const LaunchError$json = const {
  '1': 'LaunchError',
  '2': const [
    const {'1': 'error_codes', '3': 1, '4': 3, '5': 14, '6': '.multipass.LaunchError.ErrorCodes', '10': 'errorCodes'},
  ],
  '4': const [LaunchError_ErrorCodes$json],
};

@$core.Deprecated('Use launchErrorDescriptor instead')
const LaunchError_ErrorCodes$json = const {
  '1': 'ErrorCodes',
  '2': const [
    const {'1': 'OK', '2': 0},
    const {'1': 'INSTANCE_EXISTS', '2': 1},
    const {'1': 'INVALID_MEM_SIZE', '2': 2},
    const {'1': 'INVALID_DISK_SIZE', '2': 3},
    const {'1': 'INVALID_HOSTNAME', '2': 4},
    const {'1': 'INVALID_NETWORK', '2': 5},
  ],
};

/// Descriptor for `LaunchError`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List launchErrorDescriptor = $convert.base64Decode('CgtMYXVuY2hFcnJvchJCCgtlcnJvcl9jb2RlcxgBIAMoDjIhLm11bHRpcGFzcy5MYXVuY2hFcnJvci5FcnJvckNvZGVzUgplcnJvckNvZGVzIoEBCgpFcnJvckNvZGVzEgYKAk9LEAASEwoPSU5TVEFOQ0VfRVhJU1RTEAESFAoQSU5WQUxJRF9NRU1fU0laRRACEhUKEUlOVkFMSURfRElTS19TSVpFEAMSFAoQSU5WQUxJRF9IT1NUTkFNRRAEEhMKD0lOVkFMSURfTkVUV09SSxAF');
@$core.Deprecated('Use launchProgressDescriptor instead')
const LaunchProgress$json = const {
  '1': 'LaunchProgress',
  '2': const [
    const {'1': 'type', '3': 1, '4': 1, '5': 14, '6': '.multipass.LaunchProgress.ProgressTypes', '10': 'type'},
    const {'1': 'percent_complete', '3': 2, '4': 1, '5': 9, '10': 'percentComplete'},
  ],
  '4': const [LaunchProgress_ProgressTypes$json],
};

@$core.Deprecated('Use launchProgressDescriptor instead')
const LaunchProgress_ProgressTypes$json = const {
  '1': 'ProgressTypes',
  '2': const [
    const {'1': 'IMAGE', '2': 0},
    const {'1': 'KERNEL', '2': 1},
    const {'1': 'INITRD', '2': 2},
    const {'1': 'EXTRACT', '2': 3},
    const {'1': 'VERIFY', '2': 4},
    const {'1': 'WAITING', '2': 5},
  ],
};

/// Descriptor for `LaunchProgress`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List launchProgressDescriptor = $convert.base64Decode('Cg5MYXVuY2hQcm9ncmVzcxI7CgR0eXBlGAEgASgOMicubXVsdGlwYXNzLkxhdW5jaFByb2dyZXNzLlByb2dyZXNzVHlwZXNSBHR5cGUSKQoQcGVyY2VudF9jb21wbGV0ZRgCIAEoCVIPcGVyY2VudENvbXBsZXRlIlgKDVByb2dyZXNzVHlwZXMSCQoFSU1BR0UQABIKCgZLRVJORUwQARIKCgZJTklUUkQQAhILCgdFWFRSQUNUEAMSCgoGVkVSSUZZEAQSCwoHV0FJVElORxAF');
@$core.Deprecated('Use updateInfoDescriptor instead')
const UpdateInfo$json = const {
  '1': 'UpdateInfo',
  '2': const [
    const {'1': 'version', '3': 1, '4': 1, '5': 9, '10': 'version'},
    const {'1': 'url', '3': 2, '4': 1, '5': 9, '10': 'url'},
    const {'1': 'title', '3': 3, '4': 1, '5': 9, '10': 'title'},
    const {'1': 'description', '3': 4, '4': 1, '5': 9, '10': 'description'},
  ],
};

/// Descriptor for `UpdateInfo`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List updateInfoDescriptor = $convert.base64Decode('CgpVcGRhdGVJbmZvEhgKB3ZlcnNpb24YASABKAlSB3ZlcnNpb24SEAoDdXJsGAIgASgJUgN1cmwSFAoFdGl0bGUYAyABKAlSBXRpdGxlEiAKC2Rlc2NyaXB0aW9uGAQgASgJUgtkZXNjcmlwdGlvbg==');
@$core.Deprecated('Use launchReplyDescriptor instead')
const LaunchReply$json = const {
  '1': 'LaunchReply',
  '2': const [
    const {'1': 'vm_instance_name', '3': 1, '4': 1, '5': 9, '9': 0, '10': 'vmInstanceName'},
    const {'1': 'launch_progress', '3': 2, '4': 1, '5': 11, '6': '.multipass.LaunchProgress', '9': 0, '10': 'launchProgress'},
    const {'1': 'create_message', '3': 3, '4': 1, '5': 9, '9': 0, '10': 'createMessage'},
    const {'1': 'log_line', '3': 6, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'update_info', '3': 7, '4': 1, '5': 11, '6': '.multipass.UpdateInfo', '10': 'updateInfo'},
    const {'1': 'reply_message', '3': 8, '4': 1, '5': 9, '10': 'replyMessage'},
    const {'1': 'nets_need_bridging', '3': 9, '4': 3, '5': 9, '10': 'netsNeedBridging'},
    const {'1': 'aliases_to_be_created', '3': 10, '4': 3, '5': 11, '6': '.multipass.LaunchReply.Alias', '10': 'aliasesToBeCreated'},
    const {'1': 'workspaces_to_be_created', '3': 11, '4': 3, '5': 9, '10': 'workspacesToBeCreated'},
    const {'1': 'credentials_requested', '3': 12, '4': 1, '5': 8, '10': 'credentialsRequested'},
  ],
  '3': const [LaunchReply_Alias$json],
  '8': const [
    const {'1': 'create_oneof'},
  ],
};

@$core.Deprecated('Use launchReplyDescriptor instead')
const LaunchReply_Alias$json = const {
  '1': 'Alias',
  '2': const [
    const {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    const {'1': 'instance', '3': 2, '4': 1, '5': 9, '10': 'instance'},
    const {'1': 'command', '3': 3, '4': 1, '5': 9, '10': 'command'},
    const {'1': 'working_directory', '3': 4, '4': 1, '5': 9, '10': 'workingDirectory'},
  ],
};

/// Descriptor for `LaunchReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List launchReplyDescriptor = $convert.base64Decode('CgtMYXVuY2hSZXBseRIqChB2bV9pbnN0YW5jZV9uYW1lGAEgASgJSABSDnZtSW5zdGFuY2VOYW1lEkQKD2xhdW5jaF9wcm9ncmVzcxgCIAEoCzIZLm11bHRpcGFzcy5MYXVuY2hQcm9ncmVzc0gAUg5sYXVuY2hQcm9ncmVzcxInCg5jcmVhdGVfbWVzc2FnZRgDIAEoCUgAUg1jcmVhdGVNZXNzYWdlEhkKCGxvZ19saW5lGAYgASgJUgdsb2dMaW5lEjYKC3VwZGF0ZV9pbmZvGAcgASgLMhUubXVsdGlwYXNzLlVwZGF0ZUluZm9SCnVwZGF0ZUluZm8SIwoNcmVwbHlfbWVzc2FnZRgIIAEoCVIMcmVwbHlNZXNzYWdlEiwKEm5ldHNfbmVlZF9icmlkZ2luZxgJIAMoCVIQbmV0c05lZWRCcmlkZ2luZxJPChVhbGlhc2VzX3RvX2JlX2NyZWF0ZWQYCiADKAsyHC5tdWx0aXBhc3MuTGF1bmNoUmVwbHkuQWxpYXNSEmFsaWFzZXNUb0JlQ3JlYXRlZBI3Chh3b3Jrc3BhY2VzX3RvX2JlX2NyZWF0ZWQYCyADKAlSFXdvcmtzcGFjZXNUb0JlQ3JlYXRlZBIzChVjcmVkZW50aWFsc19yZXF1ZXN0ZWQYDCABKAhSFGNyZWRlbnRpYWxzUmVxdWVzdGVkGn4KBUFsaWFzEhIKBG5hbWUYASABKAlSBG5hbWUSGgoIaW5zdGFuY2UYAiABKAlSCGluc3RhbmNlEhgKB2NvbW1hbmQYAyABKAlSB2NvbW1hbmQSKwoRd29ya2luZ19kaXJlY3RvcnkYBCABKAlSEHdvcmtpbmdEaXJlY3RvcnlCDgoMY3JlYXRlX29uZW9m');
@$core.Deprecated('Use purgeRequestDescriptor instead')
const PurgeRequest$json = const {
  '1': 'PurgeRequest',
  '2': const [
    const {'1': 'verbosity_level', '3': 1, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `PurgeRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List purgeRequestDescriptor = $convert.base64Decode('CgxQdXJnZVJlcXVlc3QSJwoPdmVyYm9zaXR5X2xldmVsGAEgASgFUg52ZXJib3NpdHlMZXZlbA==');
@$core.Deprecated('Use purgeReplyDescriptor instead')
const PurgeReply$json = const {
  '1': 'PurgeReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'purged_instances', '3': 2, '4': 3, '5': 9, '10': 'purgedInstances'},
  ],
};

/// Descriptor for `PurgeReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List purgeReplyDescriptor = $convert.base64Decode('CgpQdXJnZVJlcGx5EhkKCGxvZ19saW5lGAEgASgJUgdsb2dMaW5lEikKEHB1cmdlZF9pbnN0YW5jZXMYAiADKAlSD3B1cmdlZEluc3RhbmNlcw==');
@$core.Deprecated('Use findRequestDescriptor instead')
const FindRequest$json = const {
  '1': 'FindRequest',
  '2': const [
    const {'1': 'search_string', '3': 1, '4': 1, '5': 9, '10': 'searchString'},
    const {'1': 'remote_name', '3': 2, '4': 1, '5': 9, '10': 'remoteName'},
    const {'1': 'verbosity_level', '3': 3, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'allow_unsupported', '3': 4, '4': 1, '5': 8, '10': 'allowUnsupported'},
  ],
};

/// Descriptor for `FindRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List findRequestDescriptor = $convert.base64Decode('CgtGaW5kUmVxdWVzdBIjCg1zZWFyY2hfc3RyaW5nGAEgASgJUgxzZWFyY2hTdHJpbmcSHwoLcmVtb3RlX25hbWUYAiABKAlSCnJlbW90ZU5hbWUSJwoPdmVyYm9zaXR5X2xldmVsGAMgASgFUg52ZXJib3NpdHlMZXZlbBIrChFhbGxvd191bnN1cHBvcnRlZBgEIAEoCFIQYWxsb3dVbnN1cHBvcnRlZA==');
@$core.Deprecated('Use findReplyDescriptor instead')
const FindReply$json = const {
  '1': 'FindReply',
  '2': const [
    const {'1': 'images_info', '3': 1, '4': 3, '5': 11, '6': '.multipass.FindReply.ImageInfo', '10': 'imagesInfo'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
  ],
  '3': const [FindReply_AliasInfo$json, FindReply_ImageInfo$json],
};

@$core.Deprecated('Use findReplyDescriptor instead')
const FindReply_AliasInfo$json = const {
  '1': 'AliasInfo',
  '2': const [
    const {'1': 'remote_name', '3': 1, '4': 1, '5': 9, '10': 'remoteName'},
    const {'1': 'alias', '3': 2, '4': 1, '5': 9, '10': 'alias'},
  ],
};

@$core.Deprecated('Use findReplyDescriptor instead')
const FindReply_ImageInfo$json = const {
  '1': 'ImageInfo',
  '2': const [
    const {'1': 'os', '3': 1, '4': 1, '5': 9, '10': 'os'},
    const {'1': 'release', '3': 2, '4': 1, '5': 9, '10': 'release'},
    const {'1': 'version', '3': 3, '4': 1, '5': 9, '10': 'version'},
    const {'1': 'aliases_info', '3': 4, '4': 3, '5': 11, '6': '.multipass.FindReply.AliasInfo', '10': 'aliasesInfo'},
  ],
};

/// Descriptor for `FindReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List findReplyDescriptor = $convert.base64Decode('CglGaW5kUmVwbHkSPwoLaW1hZ2VzX2luZm8YASADKAsyHi5tdWx0aXBhc3MuRmluZFJlcGx5LkltYWdlSW5mb1IKaW1hZ2VzSW5mbxIZCghsb2dfbGluZRgCIAEoCVIHbG9nTGluZRpCCglBbGlhc0luZm8SHwoLcmVtb3RlX25hbWUYASABKAlSCnJlbW90ZU5hbWUSFAoFYWxpYXMYAiABKAlSBWFsaWFzGpIBCglJbWFnZUluZm8SDgoCb3MYASABKAlSAm9zEhgKB3JlbGVhc2UYAiABKAlSB3JlbGVhc2USGAoHdmVyc2lvbhgDIAEoCVIHdmVyc2lvbhJBCgxhbGlhc2VzX2luZm8YBCADKAsyHi5tdWx0aXBhc3MuRmluZFJlcGx5LkFsaWFzSW5mb1ILYWxpYXNlc0luZm8=');
@$core.Deprecated('Use instanceNamesDescriptor instead')
const InstanceNames$json = const {
  '1': 'InstanceNames',
  '2': const [
    const {'1': 'instance_name', '3': 1, '4': 3, '5': 9, '10': 'instanceName'},
  ],
};

/// Descriptor for `InstanceNames`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List instanceNamesDescriptor = $convert.base64Decode('Cg1JbnN0YW5jZU5hbWVzEiMKDWluc3RhbmNlX25hbWUYASADKAlSDGluc3RhbmNlTmFtZQ==');
@$core.Deprecated('Use infoRequestDescriptor instead')
const InfoRequest$json = const {
  '1': 'InfoRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'no_runtime_information', '3': 3, '4': 1, '5': 8, '10': 'noRuntimeInformation'},
  ],
};

/// Descriptor for `InfoRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List infoRequestDescriptor = $convert.base64Decode('CgtJbmZvUmVxdWVzdBI/Cg5pbnN0YW5jZV9uYW1lcxgBIAEoCzIYLm11bHRpcGFzcy5JbnN0YW5jZU5hbWVzUg1pbnN0YW5jZU5hbWVzEicKD3ZlcmJvc2l0eV9sZXZlbBgCIAEoBVIOdmVyYm9zaXR5TGV2ZWwSNAoWbm9fcnVudGltZV9pbmZvcm1hdGlvbhgDIAEoCFIUbm9SdW50aW1lSW5mb3JtYXRpb24=');
@$core.Deprecated('Use idMapDescriptor instead')
const IdMap$json = const {
  '1': 'IdMap',
  '2': const [
    const {'1': 'host_id', '3': 1, '4': 1, '5': 5, '10': 'hostId'},
    const {'1': 'instance_id', '3': 2, '4': 1, '5': 5, '10': 'instanceId'},
  ],
};

/// Descriptor for `IdMap`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List idMapDescriptor = $convert.base64Decode('CgVJZE1hcBIXCgdob3N0X2lkGAEgASgFUgZob3N0SWQSHwoLaW5zdGFuY2VfaWQYAiABKAVSCmluc3RhbmNlSWQ=');
@$core.Deprecated('Use mountMapsDescriptor instead')
const MountMaps$json = const {
  '1': 'MountMaps',
  '2': const [
    const {'1': 'uid_mappings', '3': 1, '4': 3, '5': 11, '6': '.multipass.IdMap', '10': 'uidMappings'},
    const {'1': 'gid_mappings', '3': 2, '4': 3, '5': 11, '6': '.multipass.IdMap', '10': 'gidMappings'},
  ],
};

/// Descriptor for `MountMaps`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List mountMapsDescriptor = $convert.base64Decode('CglNb3VudE1hcHMSMwoMdWlkX21hcHBpbmdzGAEgAygLMhAubXVsdGlwYXNzLklkTWFwUgt1aWRNYXBwaW5ncxIzCgxnaWRfbWFwcGluZ3MYAiADKAsyEC5tdWx0aXBhc3MuSWRNYXBSC2dpZE1hcHBpbmdz');
@$core.Deprecated('Use mountInfoDescriptor instead')
const MountInfo$json = const {
  '1': 'MountInfo',
  '2': const [
    const {'1': 'longest_path_len', '3': 1, '4': 1, '5': 13, '10': 'longestPathLen'},
    const {'1': 'mount_paths', '3': 2, '4': 3, '5': 11, '6': '.multipass.MountInfo.MountPaths', '10': 'mountPaths'},
  ],
  '3': const [MountInfo_MountPaths$json],
};

@$core.Deprecated('Use mountInfoDescriptor instead')
const MountInfo_MountPaths$json = const {
  '1': 'MountPaths',
  '2': const [
    const {'1': 'source_path', '3': 1, '4': 1, '5': 9, '10': 'sourcePath'},
    const {'1': 'target_path', '3': 2, '4': 1, '5': 9, '10': 'targetPath'},
    const {'1': 'mount_maps', '3': 3, '4': 1, '5': 11, '6': '.multipass.MountMaps', '10': 'mountMaps'},
  ],
};

/// Descriptor for `MountInfo`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List mountInfoDescriptor = $convert.base64Decode('CglNb3VudEluZm8SKAoQbG9uZ2VzdF9wYXRoX2xlbhgBIAEoDVIObG9uZ2VzdFBhdGhMZW4SQAoLbW91bnRfcGF0aHMYAiADKAsyHy5tdWx0aXBhc3MuTW91bnRJbmZvLk1vdW50UGF0aHNSCm1vdW50UGF0aHMagwEKCk1vdW50UGF0aHMSHwoLc291cmNlX3BhdGgYASABKAlSCnNvdXJjZVBhdGgSHwoLdGFyZ2V0X3BhdGgYAiABKAlSCnRhcmdldFBhdGgSMwoKbW91bnRfbWFwcxgDIAEoCzIULm11bHRpcGFzcy5Nb3VudE1hcHNSCW1vdW50TWFwcw==');
@$core.Deprecated('Use instanceStatusDescriptor instead')
const InstanceStatus$json = const {
  '1': 'InstanceStatus',
  '2': const [
    const {'1': 'status', '3': 1, '4': 1, '5': 14, '6': '.multipass.InstanceStatus.Status', '10': 'status'},
  ],
  '4': const [InstanceStatus_Status$json],
};

@$core.Deprecated('Use instanceStatusDescriptor instead')
const InstanceStatus_Status$json = const {
  '1': 'Status',
  '2': const [
    const {'1': 'UNKNOWN', '2': 0},
    const {'1': 'RUNNING', '2': 1},
    const {'1': 'STARTING', '2': 2},
    const {'1': 'RESTARTING', '2': 3},
    const {'1': 'STOPPED', '2': 4},
    const {'1': 'DELETED', '2': 5},
    const {'1': 'DELAYED_SHUTDOWN', '2': 6},
    const {'1': 'SUSPENDING', '2': 7},
    const {'1': 'SUSPENDED', '2': 8},
  ],
};

/// Descriptor for `InstanceStatus`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List instanceStatusDescriptor = $convert.base64Decode('Cg5JbnN0YW5jZVN0YXR1cxI4CgZzdGF0dXMYASABKA4yIC5tdWx0aXBhc3MuSW5zdGFuY2VTdGF0dXMuU3RhdHVzUgZzdGF0dXMijwEKBlN0YXR1cxILCgdVTktOT1dOEAASCwoHUlVOTklORxABEgwKCFNUQVJUSU5HEAISDgoKUkVTVEFSVElORxADEgsKB1NUT1BQRUQQBBILCgdERUxFVEVEEAUSFAoQREVMQVlFRF9TSFVURE9XThAGEg4KClNVU1BFTkRJTkcQBxINCglTVVNQRU5ERUQQCA==');
@$core.Deprecated('Use infoReplyDescriptor instead')
const InfoReply$json = const {
  '1': 'InfoReply',
  '2': const [
    const {'1': 'info', '3': 1, '4': 3, '5': 11, '6': '.multipass.InfoReply.Info', '10': 'info'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
  ],
  '3': const [InfoReply_Info$json],
};

@$core.Deprecated('Use infoReplyDescriptor instead')
const InfoReply_Info$json = const {
  '1': 'Info',
  '2': const [
    const {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    const {'1': 'instance_status', '3': 2, '4': 1, '5': 11, '6': '.multipass.InstanceStatus', '10': 'instanceStatus'},
    const {'1': 'image_release', '3': 3, '4': 1, '5': 9, '10': 'imageRelease'},
    const {'1': 'current_release', '3': 4, '4': 1, '5': 9, '10': 'currentRelease'},
    const {'1': 'id', '3': 5, '4': 1, '5': 9, '10': 'id'},
    const {'1': 'load', '3': 6, '4': 1, '5': 9, '10': 'load'},
    const {'1': 'memory_usage', '3': 7, '4': 1, '5': 9, '10': 'memoryUsage'},
    const {'1': 'memory_total', '3': 8, '4': 1, '5': 9, '10': 'memoryTotal'},
    const {'1': 'disk_usage', '3': 9, '4': 1, '5': 9, '10': 'diskUsage'},
    const {'1': 'disk_total', '3': 10, '4': 1, '5': 9, '10': 'diskTotal'},
    const {'1': 'ipv4', '3': 11, '4': 3, '5': 9, '10': 'ipv4'},
    const {'1': 'ipv6', '3': 12, '4': 3, '5': 9, '10': 'ipv6'},
    const {'1': 'mount_info', '3': 13, '4': 1, '5': 11, '6': '.multipass.MountInfo', '10': 'mountInfo'},
    const {'1': 'cpu_count', '3': 14, '4': 1, '5': 9, '10': 'cpuCount'},
  ],
};

/// Descriptor for `InfoReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List infoReplyDescriptor = $convert.base64Decode('CglJbmZvUmVwbHkSLQoEaW5mbxgBIAMoCzIZLm11bHRpcGFzcy5JbmZvUmVwbHkuSW5mb1IEaW5mbxIZCghsb2dfbGluZRgCIAEoCVIHbG9nTGluZRrOAwoESW5mbxISCgRuYW1lGAEgASgJUgRuYW1lEkIKD2luc3RhbmNlX3N0YXR1cxgCIAEoCzIZLm11bHRpcGFzcy5JbnN0YW5jZVN0YXR1c1IOaW5zdGFuY2VTdGF0dXMSIwoNaW1hZ2VfcmVsZWFzZRgDIAEoCVIMaW1hZ2VSZWxlYXNlEicKD2N1cnJlbnRfcmVsZWFzZRgEIAEoCVIOY3VycmVudFJlbGVhc2USDgoCaWQYBSABKAlSAmlkEhIKBGxvYWQYBiABKAlSBGxvYWQSIQoMbWVtb3J5X3VzYWdlGAcgASgJUgttZW1vcnlVc2FnZRIhCgxtZW1vcnlfdG90YWwYCCABKAlSC21lbW9yeVRvdGFsEh0KCmRpc2tfdXNhZ2UYCSABKAlSCWRpc2tVc2FnZRIdCgpkaXNrX3RvdGFsGAogASgJUglkaXNrVG90YWwSEgoEaXB2NBgLIAMoCVIEaXB2NBISCgRpcHY2GAwgAygJUgRpcHY2EjMKCm1vdW50X2luZm8YDSABKAsyFC5tdWx0aXBhc3MuTW91bnRJbmZvUgltb3VudEluZm8SGwoJY3B1X2NvdW50GA4gASgJUghjcHVDb3VudA==');
@$core.Deprecated('Use listRequestDescriptor instead')
const ListRequest$json = const {
  '1': 'ListRequest',
  '2': const [
    const {'1': 'verbosity_level', '3': 1, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'request_ipv4', '3': 2, '4': 1, '5': 8, '10': 'requestIpv4'},
  ],
};

/// Descriptor for `ListRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List listRequestDescriptor = $convert.base64Decode('CgtMaXN0UmVxdWVzdBInCg92ZXJib3NpdHlfbGV2ZWwYASABKAVSDnZlcmJvc2l0eUxldmVsEiEKDHJlcXVlc3RfaXB2NBgCIAEoCFILcmVxdWVzdElwdjQ=');
@$core.Deprecated('Use listVMInstanceDescriptor instead')
const ListVMInstance$json = const {
  '1': 'ListVMInstance',
  '2': const [
    const {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    const {'1': 'instance_status', '3': 2, '4': 1, '5': 11, '6': '.multipass.InstanceStatus', '10': 'instanceStatus'},
    const {'1': 'ipv4', '3': 3, '4': 3, '5': 9, '10': 'ipv4'},
    const {'1': 'ipv6', '3': 4, '4': 3, '5': 9, '10': 'ipv6'},
    const {'1': 'current_release', '3': 5, '4': 1, '5': 9, '10': 'currentRelease'},
  ],
};

/// Descriptor for `ListVMInstance`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List listVMInstanceDescriptor = $convert.base64Decode('Cg5MaXN0Vk1JbnN0YW5jZRISCgRuYW1lGAEgASgJUgRuYW1lEkIKD2luc3RhbmNlX3N0YXR1cxgCIAEoCzIZLm11bHRpcGFzcy5JbnN0YW5jZVN0YXR1c1IOaW5zdGFuY2VTdGF0dXMSEgoEaXB2NBgDIAMoCVIEaXB2NBISCgRpcHY2GAQgAygJUgRpcHY2EicKD2N1cnJlbnRfcmVsZWFzZRgFIAEoCVIOY3VycmVudFJlbGVhc2U=');
@$core.Deprecated('Use listReplyDescriptor instead')
const ListReply$json = const {
  '1': 'ListReply',
  '2': const [
    const {'1': 'instances', '3': 1, '4': 3, '5': 11, '6': '.multipass.ListVMInstance', '10': 'instances'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'update_info', '3': 3, '4': 1, '5': 11, '6': '.multipass.UpdateInfo', '10': 'updateInfo'},
  ],
};

/// Descriptor for `ListReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List listReplyDescriptor = $convert.base64Decode('CglMaXN0UmVwbHkSNwoJaW5zdGFuY2VzGAEgAygLMhkubXVsdGlwYXNzLkxpc3RWTUluc3RhbmNlUglpbnN0YW5jZXMSGQoIbG9nX2xpbmUYAiABKAlSB2xvZ0xpbmUSNgoLdXBkYXRlX2luZm8YAyABKAsyFS5tdWx0aXBhc3MuVXBkYXRlSW5mb1IKdXBkYXRlSW5mbw==');
@$core.Deprecated('Use networksRequestDescriptor instead')
const NetworksRequest$json = const {
  '1': 'NetworksRequest',
  '2': const [
    const {'1': 'verbosity_level', '3': 1, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `NetworksRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List networksRequestDescriptor = $convert.base64Decode('Cg9OZXR3b3Jrc1JlcXVlc3QSJwoPdmVyYm9zaXR5X2xldmVsGAEgASgFUg52ZXJib3NpdHlMZXZlbA==');
@$core.Deprecated('Use netInterfaceDescriptor instead')
const NetInterface$json = const {
  '1': 'NetInterface',
  '2': const [
    const {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    const {'1': 'type', '3': 2, '4': 1, '5': 9, '10': 'type'},
    const {'1': 'description', '3': 3, '4': 1, '5': 9, '10': 'description'},
  ],
};

/// Descriptor for `NetInterface`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List netInterfaceDescriptor = $convert.base64Decode('CgxOZXRJbnRlcmZhY2USEgoEbmFtZRgBIAEoCVIEbmFtZRISCgR0eXBlGAIgASgJUgR0eXBlEiAKC2Rlc2NyaXB0aW9uGAMgASgJUgtkZXNjcmlwdGlvbg==');
@$core.Deprecated('Use networksReplyDescriptor instead')
const NetworksReply$json = const {
  '1': 'NetworksReply',
  '2': const [
    const {'1': 'interfaces', '3': 1, '4': 3, '5': 11, '6': '.multipass.NetInterface', '10': 'interfaces'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'update_info', '3': 3, '4': 1, '5': 11, '6': '.multipass.UpdateInfo', '10': 'updateInfo'},
  ],
};

/// Descriptor for `NetworksReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List networksReplyDescriptor = $convert.base64Decode('Cg1OZXR3b3Jrc1JlcGx5EjcKCmludGVyZmFjZXMYASADKAsyFy5tdWx0aXBhc3MuTmV0SW50ZXJmYWNlUgppbnRlcmZhY2VzEhkKCGxvZ19saW5lGAIgASgJUgdsb2dMaW5lEjYKC3VwZGF0ZV9pbmZvGAMgASgLMhUubXVsdGlwYXNzLlVwZGF0ZUluZm9SCnVwZGF0ZUluZm8=');
@$core.Deprecated('Use targetPathInfoDescriptor instead')
const TargetPathInfo$json = const {
  '1': 'TargetPathInfo',
  '2': const [
    const {'1': 'instance_name', '3': 1, '4': 1, '5': 9, '10': 'instanceName'},
    const {'1': 'target_path', '3': 2, '4': 1, '5': 9, '10': 'targetPath'},
  ],
};

/// Descriptor for `TargetPathInfo`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List targetPathInfoDescriptor = $convert.base64Decode('Cg5UYXJnZXRQYXRoSW5mbxIjCg1pbnN0YW5jZV9uYW1lGAEgASgJUgxpbnN0YW5jZU5hbWUSHwoLdGFyZ2V0X3BhdGgYAiABKAlSCnRhcmdldFBhdGg=');
@$core.Deprecated('Use userCredentialsDescriptor instead')
const UserCredentials$json = const {
  '1': 'UserCredentials',
  '2': const [
    const {'1': 'username', '3': 1, '4': 1, '5': 9, '10': 'username'},
    const {'1': 'password', '3': 2, '4': 1, '5': 9, '10': 'password'},
  ],
};

/// Descriptor for `UserCredentials`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List userCredentialsDescriptor = $convert.base64Decode('Cg9Vc2VyQ3JlZGVudGlhbHMSGgoIdXNlcm5hbWUYASABKAlSCHVzZXJuYW1lEhoKCHBhc3N3b3JkGAIgASgJUghwYXNzd29yZA==');
@$core.Deprecated('Use mountRequestDescriptor instead')
const MountRequest$json = const {
  '1': 'MountRequest',
  '2': const [
    const {'1': 'source_path', '3': 1, '4': 1, '5': 9, '10': 'sourcePath'},
    const {'1': 'target_paths', '3': 2, '4': 3, '5': 11, '6': '.multipass.TargetPathInfo', '10': 'targetPaths'},
    const {'1': 'mount_maps', '3': 3, '4': 1, '5': 11, '6': '.multipass.MountMaps', '10': 'mountMaps'},
    const {'1': 'verbosity_level', '3': 4, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'mount_type', '3': 5, '4': 1, '5': 14, '6': '.multipass.MountRequest.MountType', '10': 'mountType'},
    const {'1': 'user_credentials', '3': 6, '4': 1, '5': 11, '6': '.multipass.UserCredentials', '10': 'userCredentials'},
  ],
  '4': const [MountRequest_MountType$json],
};

@$core.Deprecated('Use mountRequestDescriptor instead')
const MountRequest_MountType$json = const {
  '1': 'MountType',
  '2': const [
    const {'1': 'CLASSIC', '2': 0},
    const {'1': 'NATIVE', '2': 1},
  ],
};

/// Descriptor for `MountRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List mountRequestDescriptor = $convert.base64Decode('CgxNb3VudFJlcXVlc3QSHwoLc291cmNlX3BhdGgYASABKAlSCnNvdXJjZVBhdGgSPAoMdGFyZ2V0X3BhdGhzGAIgAygLMhkubXVsdGlwYXNzLlRhcmdldFBhdGhJbmZvUgt0YXJnZXRQYXRocxIzCgptb3VudF9tYXBzGAMgASgLMhQubXVsdGlwYXNzLk1vdW50TWFwc1IJbW91bnRNYXBzEicKD3ZlcmJvc2l0eV9sZXZlbBgEIAEoBVIOdmVyYm9zaXR5TGV2ZWwSQAoKbW91bnRfdHlwZRgFIAEoDjIhLm11bHRpcGFzcy5Nb3VudFJlcXVlc3QuTW91bnRUeXBlUgltb3VudFR5cGUSRQoQdXNlcl9jcmVkZW50aWFscxgGIAEoCzIaLm11bHRpcGFzcy5Vc2VyQ3JlZGVudGlhbHNSD3VzZXJDcmVkZW50aWFscyIkCglNb3VudFR5cGUSCwoHQ0xBU1NJQxAAEgoKBk5BVElWRRAB');
@$core.Deprecated('Use mountReplyDescriptor instead')
const MountReply$json = const {
  '1': 'MountReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'reply_message', '3': 2, '4': 1, '5': 9, '10': 'replyMessage'},
    const {'1': 'credentials_requested', '3': 3, '4': 1, '5': 8, '10': 'credentialsRequested'},
  ],
};

/// Descriptor for `MountReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List mountReplyDescriptor = $convert.base64Decode('CgpNb3VudFJlcGx5EhkKCGxvZ19saW5lGAEgASgJUgdsb2dMaW5lEiMKDXJlcGx5X21lc3NhZ2UYAiABKAlSDHJlcGx5TWVzc2FnZRIzChVjcmVkZW50aWFsc19yZXF1ZXN0ZWQYAyABKAhSFGNyZWRlbnRpYWxzUmVxdWVzdGVk');
@$core.Deprecated('Use pingRequestDescriptor instead')
const PingRequest$json = const {
  '1': 'PingRequest',
};

/// Descriptor for `PingRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List pingRequestDescriptor = $convert.base64Decode('CgtQaW5nUmVxdWVzdA==');
@$core.Deprecated('Use pingReplyDescriptor instead')
const PingReply$json = const {
  '1': 'PingReply',
};

/// Descriptor for `PingReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List pingReplyDescriptor = $convert.base64Decode('CglQaW5nUmVwbHk=');
@$core.Deprecated('Use recoverRequestDescriptor instead')
const RecoverRequest$json = const {
  '1': 'RecoverRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `RecoverRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List recoverRequestDescriptor = $convert.base64Decode('Cg5SZWNvdmVyUmVxdWVzdBI/Cg5pbnN0YW5jZV9uYW1lcxgBIAEoCzIYLm11bHRpcGFzcy5JbnN0YW5jZU5hbWVzUg1pbnN0YW5jZU5hbWVzEicKD3ZlcmJvc2l0eV9sZXZlbBgCIAEoBVIOdmVyYm9zaXR5TGV2ZWw=');
@$core.Deprecated('Use recoverReplyDescriptor instead')
const RecoverReply$json = const {
  '1': 'RecoverReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
  ],
};

/// Descriptor for `RecoverReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List recoverReplyDescriptor = $convert.base64Decode('CgxSZWNvdmVyUmVwbHkSGQoIbG9nX2xpbmUYASABKAlSB2xvZ0xpbmU=');
@$core.Deprecated('Use sSHInfoRequestDescriptor instead')
const SSHInfoRequest$json = const {
  '1': 'SSHInfoRequest',
  '2': const [
    const {'1': 'instance_name', '3': 1, '4': 3, '5': 9, '10': 'instanceName'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `SSHInfoRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List sSHInfoRequestDescriptor = $convert.base64Decode('Cg5TU0hJbmZvUmVxdWVzdBIjCg1pbnN0YW5jZV9uYW1lGAEgAygJUgxpbnN0YW5jZU5hbWUSJwoPdmVyYm9zaXR5X2xldmVsGAIgASgFUg52ZXJib3NpdHlMZXZlbA==');
@$core.Deprecated('Use sSHInfoDescriptor instead')
const SSHInfo$json = const {
  '1': 'SSHInfo',
  '2': const [
    const {'1': 'port', '3': 1, '4': 1, '5': 5, '10': 'port'},
    const {'1': 'priv_key_base64', '3': 2, '4': 1, '5': 9, '10': 'privKeyBase64'},
    const {'1': 'host', '3': 3, '4': 1, '5': 9, '10': 'host'},
    const {'1': 'username', '3': 4, '4': 1, '5': 9, '10': 'username'},
  ],
};

/// Descriptor for `SSHInfo`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List sSHInfoDescriptor = $convert.base64Decode('CgdTU0hJbmZvEhIKBHBvcnQYASABKAVSBHBvcnQSJgoPcHJpdl9rZXlfYmFzZTY0GAIgASgJUg1wcml2S2V5QmFzZTY0EhIKBGhvc3QYAyABKAlSBGhvc3QSGgoIdXNlcm5hbWUYBCABKAlSCHVzZXJuYW1l');
@$core.Deprecated('Use sSHInfoReplyDescriptor instead')
const SSHInfoReply$json = const {
  '1': 'SSHInfoReply',
  '2': const [
    const {'1': 'ssh_info', '3': 1, '4': 3, '5': 11, '6': '.multipass.SSHInfoReply.SshInfoEntry', '10': 'sshInfo'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
  ],
  '3': const [SSHInfoReply_SshInfoEntry$json],
};

@$core.Deprecated('Use sSHInfoReplyDescriptor instead')
const SSHInfoReply_SshInfoEntry$json = const {
  '1': 'SshInfoEntry',
  '2': const [
    const {'1': 'key', '3': 1, '4': 1, '5': 9, '10': 'key'},
    const {'1': 'value', '3': 2, '4': 1, '5': 11, '6': '.multipass.SSHInfo', '10': 'value'},
  ],
  '7': const {'7': true},
};

/// Descriptor for `SSHInfoReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List sSHInfoReplyDescriptor = $convert.base64Decode('CgxTU0hJbmZvUmVwbHkSPwoIc3NoX2luZm8YASADKAsyJC5tdWx0aXBhc3MuU1NISW5mb1JlcGx5LlNzaEluZm9FbnRyeVIHc3NoSW5mbxIZCghsb2dfbGluZRgCIAEoCVIHbG9nTGluZRpOCgxTc2hJbmZvRW50cnkSEAoDa2V5GAEgASgJUgNrZXkSKAoFdmFsdWUYAiABKAsyEi5tdWx0aXBhc3MuU1NISW5mb1IFdmFsdWU6AjgB');
@$core.Deprecated('Use startErrorDescriptor instead')
const StartError$json = const {
  '1': 'StartError',
  '2': const [
    const {'1': 'instance_errors', '3': 1, '4': 3, '5': 11, '6': '.multipass.StartError.InstanceErrorsEntry', '10': 'instanceErrors'},
  ],
  '3': const [StartError_InstanceErrorsEntry$json],
  '4': const [StartError_ErrorCode$json],
};

@$core.Deprecated('Use startErrorDescriptor instead')
const StartError_InstanceErrorsEntry$json = const {
  '1': 'InstanceErrorsEntry',
  '2': const [
    const {'1': 'key', '3': 1, '4': 1, '5': 9, '10': 'key'},
    const {'1': 'value', '3': 2, '4': 1, '5': 14, '6': '.multipass.StartError.ErrorCode', '10': 'value'},
  ],
  '7': const {'7': true},
};

@$core.Deprecated('Use startErrorDescriptor instead')
const StartError_ErrorCode$json = const {
  '1': 'ErrorCode',
  '2': const [
    const {'1': 'OK', '2': 0},
    const {'1': 'DOES_NOT_EXIST', '2': 1},
    const {'1': 'INSTANCE_DELETED', '2': 2},
    const {'1': 'OTHER', '2': 3},
  ],
};

/// Descriptor for `StartError`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List startErrorDescriptor = $convert.base64Decode('CgpTdGFydEVycm9yElIKD2luc3RhbmNlX2Vycm9ycxgBIAMoCzIpLm11bHRpcGFzcy5TdGFydEVycm9yLkluc3RhbmNlRXJyb3JzRW50cnlSDmluc3RhbmNlRXJyb3JzGmIKE0luc3RhbmNlRXJyb3JzRW50cnkSEAoDa2V5GAEgASgJUgNrZXkSNQoFdmFsdWUYAiABKA4yHy5tdWx0aXBhc3MuU3RhcnRFcnJvci5FcnJvckNvZGVSBXZhbHVlOgI4ASJICglFcnJvckNvZGUSBgoCT0sQABISCg5ET0VTX05PVF9FWElTVBABEhQKEElOU1RBTkNFX0RFTEVURUQQAhIJCgVPVEhFUhAD');
@$core.Deprecated('Use startRequestDescriptor instead')
const StartRequest$json = const {
  '1': 'StartRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'timeout', '3': 3, '4': 1, '5': 5, '10': 'timeout'},
    const {'1': 'user_credentials', '3': 4, '4': 1, '5': 11, '6': '.multipass.UserCredentials', '10': 'userCredentials'},
  ],
};

/// Descriptor for `StartRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List startRequestDescriptor = $convert.base64Decode('CgxTdGFydFJlcXVlc3QSPwoOaW5zdGFuY2VfbmFtZXMYASABKAsyGC5tdWx0aXBhc3MuSW5zdGFuY2VOYW1lc1INaW5zdGFuY2VOYW1lcxInCg92ZXJib3NpdHlfbGV2ZWwYAiABKAVSDnZlcmJvc2l0eUxldmVsEhgKB3RpbWVvdXQYAyABKAVSB3RpbWVvdXQSRQoQdXNlcl9jcmVkZW50aWFscxgEIAEoCzIaLm11bHRpcGFzcy5Vc2VyQ3JlZGVudGlhbHNSD3VzZXJDcmVkZW50aWFscw==');
@$core.Deprecated('Use startReplyDescriptor instead')
const StartReply$json = const {
  '1': 'StartReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'reply_message', '3': 2, '4': 1, '5': 9, '10': 'replyMessage'},
    const {'1': 'update_info', '3': 3, '4': 1, '5': 11, '6': '.multipass.UpdateInfo', '10': 'updateInfo'},
    const {'1': 'credentials_requested', '3': 4, '4': 1, '5': 8, '10': 'credentialsRequested'},
  ],
};

/// Descriptor for `StartReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List startReplyDescriptor = $convert.base64Decode('CgpTdGFydFJlcGx5EhkKCGxvZ19saW5lGAEgASgJUgdsb2dMaW5lEiMKDXJlcGx5X21lc3NhZ2UYAiABKAlSDHJlcGx5TWVzc2FnZRI2Cgt1cGRhdGVfaW5mbxgDIAEoCzIVLm11bHRpcGFzcy5VcGRhdGVJbmZvUgp1cGRhdGVJbmZvEjMKFWNyZWRlbnRpYWxzX3JlcXVlc3RlZBgEIAEoCFIUY3JlZGVudGlhbHNSZXF1ZXN0ZWQ=');
@$core.Deprecated('Use stopRequestDescriptor instead')
const StopRequest$json = const {
  '1': 'StopRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'time_minutes', '3': 2, '4': 1, '5': 5, '10': 'timeMinutes'},
    const {'1': 'cancel_shutdown', '3': 3, '4': 1, '5': 8, '10': 'cancelShutdown'},
    const {'1': 'verbosity_level', '3': 4, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `StopRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List stopRequestDescriptor = $convert.base64Decode('CgtTdG9wUmVxdWVzdBI/Cg5pbnN0YW5jZV9uYW1lcxgBIAEoCzIYLm11bHRpcGFzcy5JbnN0YW5jZU5hbWVzUg1pbnN0YW5jZU5hbWVzEiEKDHRpbWVfbWludXRlcxgCIAEoBVILdGltZU1pbnV0ZXMSJwoPY2FuY2VsX3NodXRkb3duGAMgASgIUg5jYW5jZWxTaHV0ZG93bhInCg92ZXJib3NpdHlfbGV2ZWwYBCABKAVSDnZlcmJvc2l0eUxldmVs');
@$core.Deprecated('Use stopReplyDescriptor instead')
const StopReply$json = const {
  '1': 'StopReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
  ],
};

/// Descriptor for `StopReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List stopReplyDescriptor = $convert.base64Decode('CglTdG9wUmVwbHkSGQoIbG9nX2xpbmUYASABKAlSB2xvZ0xpbmU=');
@$core.Deprecated('Use suspendRequestDescriptor instead')
const SuspendRequest$json = const {
  '1': 'SuspendRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `SuspendRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List suspendRequestDescriptor = $convert.base64Decode('Cg5TdXNwZW5kUmVxdWVzdBI/Cg5pbnN0YW5jZV9uYW1lcxgBIAEoCzIYLm11bHRpcGFzcy5JbnN0YW5jZU5hbWVzUg1pbnN0YW5jZU5hbWVzEicKD3ZlcmJvc2l0eV9sZXZlbBgCIAEoBVIOdmVyYm9zaXR5TGV2ZWw=');
@$core.Deprecated('Use suspendReplyDescriptor instead')
const SuspendReply$json = const {
  '1': 'SuspendReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
  ],
};

/// Descriptor for `SuspendReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List suspendReplyDescriptor = $convert.base64Decode('CgxTdXNwZW5kUmVwbHkSGQoIbG9nX2xpbmUYASABKAlSB2xvZ0xpbmU=');
@$core.Deprecated('Use restartRequestDescriptor instead')
const RestartRequest$json = const {
  '1': 'RestartRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
    const {'1': 'timeout', '3': 3, '4': 1, '5': 5, '10': 'timeout'},
    const {'1': 'user_credentials', '3': 4, '4': 1, '5': 11, '6': '.multipass.UserCredentials', '10': 'userCredentials'},
  ],
};

/// Descriptor for `RestartRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List restartRequestDescriptor = $convert.base64Decode('Cg5SZXN0YXJ0UmVxdWVzdBI/Cg5pbnN0YW5jZV9uYW1lcxgBIAEoCzIYLm11bHRpcGFzcy5JbnN0YW5jZU5hbWVzUg1pbnN0YW5jZU5hbWVzEicKD3ZlcmJvc2l0eV9sZXZlbBgCIAEoBVIOdmVyYm9zaXR5TGV2ZWwSGAoHdGltZW91dBgDIAEoBVIHdGltZW91dBJFChB1c2VyX2NyZWRlbnRpYWxzGAQgASgLMhoubXVsdGlwYXNzLlVzZXJDcmVkZW50aWFsc1IPdXNlckNyZWRlbnRpYWxz');
@$core.Deprecated('Use restartReplyDescriptor instead')
const RestartReply$json = const {
  '1': 'RestartReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'reply_message', '3': 2, '4': 1, '5': 9, '10': 'replyMessage'},
    const {'1': 'update_info', '3': 3, '4': 1, '5': 11, '6': '.multipass.UpdateInfo', '10': 'updateInfo'},
    const {'1': 'credentials_requested', '3': 4, '4': 1, '5': 8, '10': 'credentialsRequested'},
  ],
};

/// Descriptor for `RestartReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List restartReplyDescriptor = $convert.base64Decode('CgxSZXN0YXJ0UmVwbHkSGQoIbG9nX2xpbmUYASABKAlSB2xvZ0xpbmUSIwoNcmVwbHlfbWVzc2FnZRgCIAEoCVIMcmVwbHlNZXNzYWdlEjYKC3VwZGF0ZV9pbmZvGAMgASgLMhUubXVsdGlwYXNzLlVwZGF0ZUluZm9SCnVwZGF0ZUluZm8SMwoVY3JlZGVudGlhbHNfcmVxdWVzdGVkGAQgASgIUhRjcmVkZW50aWFsc1JlcXVlc3RlZA==');
@$core.Deprecated('Use deleteRequestDescriptor instead')
const DeleteRequest$json = const {
  '1': 'DeleteRequest',
  '2': const [
    const {'1': 'instance_names', '3': 1, '4': 1, '5': 11, '6': '.multipass.InstanceNames', '10': 'instanceNames'},
    const {'1': 'purge', '3': 2, '4': 1, '5': 8, '10': 'purge'},
    const {'1': 'verbosity_level', '3': 3, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `DeleteRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List deleteRequestDescriptor = $convert.base64Decode('Cg1EZWxldGVSZXF1ZXN0Ej8KDmluc3RhbmNlX25hbWVzGAEgASgLMhgubXVsdGlwYXNzLkluc3RhbmNlTmFtZXNSDWluc3RhbmNlTmFtZXMSFAoFcHVyZ2UYAiABKAhSBXB1cmdlEicKD3ZlcmJvc2l0eV9sZXZlbBgDIAEoBVIOdmVyYm9zaXR5TGV2ZWw=');
@$core.Deprecated('Use deleteReplyDescriptor instead')
const DeleteReply$json = const {
  '1': 'DeleteReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'purged_instances', '3': 2, '4': 3, '5': 9, '10': 'purgedInstances'},
  ],
};

/// Descriptor for `DeleteReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List deleteReplyDescriptor = $convert.base64Decode('CgtEZWxldGVSZXBseRIZCghsb2dfbGluZRgBIAEoCVIHbG9nTGluZRIpChBwdXJnZWRfaW5zdGFuY2VzGAIgAygJUg9wdXJnZWRJbnN0YW5jZXM=');
@$core.Deprecated('Use umountRequestDescriptor instead')
const UmountRequest$json = const {
  '1': 'UmountRequest',
  '2': const [
    const {'1': 'target_paths', '3': 1, '4': 3, '5': 11, '6': '.multipass.TargetPathInfo', '10': 'targetPaths'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `UmountRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List umountRequestDescriptor = $convert.base64Decode('Cg1VbW91bnRSZXF1ZXN0EjwKDHRhcmdldF9wYXRocxgBIAMoCzIZLm11bHRpcGFzcy5UYXJnZXRQYXRoSW5mb1ILdGFyZ2V0UGF0aHMSJwoPdmVyYm9zaXR5X2xldmVsGAIgASgFUg52ZXJib3NpdHlMZXZlbA==');
@$core.Deprecated('Use umountReplyDescriptor instead')
const UmountReply$json = const {
  '1': 'UmountReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
  ],
};

/// Descriptor for `UmountReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List umountReplyDescriptor = $convert.base64Decode('CgtVbW91bnRSZXBseRIZCghsb2dfbGluZRgBIAEoCVIHbG9nTGluZQ==');
@$core.Deprecated('Use versionRequestDescriptor instead')
const VersionRequest$json = const {
  '1': 'VersionRequest',
  '2': const [
    const {'1': 'verbosity_level', '3': 1, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `VersionRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List versionRequestDescriptor = $convert.base64Decode('Cg5WZXJzaW9uUmVxdWVzdBInCg92ZXJib3NpdHlfbGV2ZWwYASABKAVSDnZlcmJvc2l0eUxldmVs');
@$core.Deprecated('Use versionReplyDescriptor instead')
const VersionReply$json = const {
  '1': 'VersionReply',
  '2': const [
    const {'1': 'version', '3': 1, '4': 1, '5': 9, '10': 'version'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'update_info', '3': 3, '4': 1, '5': 11, '6': '.multipass.UpdateInfo', '10': 'updateInfo'},
  ],
};

/// Descriptor for `VersionReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List versionReplyDescriptor = $convert.base64Decode('CgxWZXJzaW9uUmVwbHkSGAoHdmVyc2lvbhgBIAEoCVIHdmVyc2lvbhIZCghsb2dfbGluZRgCIAEoCVIHbG9nTGluZRI2Cgt1cGRhdGVfaW5mbxgDIAEoCzIVLm11bHRpcGFzcy5VcGRhdGVJbmZvUgp1cGRhdGVJbmZv');
@$core.Deprecated('Use getRequestDescriptor instead')
const GetRequest$json = const {
  '1': 'GetRequest',
  '2': const [
    const {'1': 'key', '3': 1, '4': 1, '5': 9, '10': 'key'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `GetRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getRequestDescriptor = $convert.base64Decode('CgpHZXRSZXF1ZXN0EhAKA2tleRgBIAEoCVIDa2V5EicKD3ZlcmJvc2l0eV9sZXZlbBgCIAEoBVIOdmVyYm9zaXR5TGV2ZWw=');
@$core.Deprecated('Use getReplyDescriptor instead')
const GetReply$json = const {
  '1': 'GetReply',
  '2': const [
    const {'1': 'value', '3': 1, '4': 1, '5': 9, '10': 'value'},
    const {'1': 'log_line', '3': 2, '4': 1, '5': 9, '10': 'logLine'},
  ],
};

/// Descriptor for `GetReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List getReplyDescriptor = $convert.base64Decode('CghHZXRSZXBseRIUCgV2YWx1ZRgBIAEoCVIFdmFsdWUSGQoIbG9nX2xpbmUYAiABKAlSB2xvZ0xpbmU=');
@$core.Deprecated('Use setRequestDescriptor instead')
const SetRequest$json = const {
  '1': 'SetRequest',
  '2': const [
    const {'1': 'key', '3': 1, '4': 1, '5': 9, '10': 'key'},
    const {'1': 'val', '3': 2, '4': 1, '5': 9, '10': 'val'},
    const {'1': 'verbosity_level', '3': 3, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `SetRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setRequestDescriptor = $convert.base64Decode('CgpTZXRSZXF1ZXN0EhAKA2tleRgBIAEoCVIDa2V5EhAKA3ZhbBgCIAEoCVIDdmFsEicKD3ZlcmJvc2l0eV9sZXZlbBgDIAEoBVIOdmVyYm9zaXR5TGV2ZWw=');
@$core.Deprecated('Use setReplyDescriptor instead')
const SetReply$json = const {
  '1': 'SetReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'reply_message', '3': 2, '4': 1, '5': 9, '10': 'replyMessage'},
  ],
};

/// Descriptor for `SetReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List setReplyDescriptor = $convert.base64Decode('CghTZXRSZXBseRIZCghsb2dfbGluZRgBIAEoCVIHbG9nTGluZRIjCg1yZXBseV9tZXNzYWdlGAIgASgJUgxyZXBseU1lc3NhZ2U=');
@$core.Deprecated('Use keysRequestDescriptor instead')
const KeysRequest$json = const {
  '1': 'KeysRequest',
  '2': const [
    const {'1': 'verbosity_level', '3': 3, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `KeysRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List keysRequestDescriptor = $convert.base64Decode('CgtLZXlzUmVxdWVzdBInCg92ZXJib3NpdHlfbGV2ZWwYAyABKAVSDnZlcmJvc2l0eUxldmVs');
@$core.Deprecated('Use keysReplyDescriptor instead')
const KeysReply$json = const {
  '1': 'KeysReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
    const {'1': 'settings_keys', '3': 2, '4': 3, '5': 9, '10': 'settingsKeys'},
  ],
};

/// Descriptor for `KeysReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List keysReplyDescriptor = $convert.base64Decode('CglLZXlzUmVwbHkSGQoIbG9nX2xpbmUYASABKAlSB2xvZ0xpbmUSIwoNc2V0dGluZ3Nfa2V5cxgCIAMoCVIMc2V0dGluZ3NLZXlz');
@$core.Deprecated('Use authenticateRequestDescriptor instead')
const AuthenticateRequest$json = const {
  '1': 'AuthenticateRequest',
  '2': const [
    const {'1': 'passphrase', '3': 1, '4': 1, '5': 9, '10': 'passphrase'},
    const {'1': 'verbosity_level', '3': 2, '4': 1, '5': 5, '10': 'verbosityLevel'},
  ],
};

/// Descriptor for `AuthenticateRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List authenticateRequestDescriptor = $convert.base64Decode('ChNBdXRoZW50aWNhdGVSZXF1ZXN0Eh4KCnBhc3NwaHJhc2UYASABKAlSCnBhc3NwaHJhc2USJwoPdmVyYm9zaXR5X2xldmVsGAIgASgFUg52ZXJib3NpdHlMZXZlbA==');
@$core.Deprecated('Use authenticateReplyDescriptor instead')
const AuthenticateReply$json = const {
  '1': 'AuthenticateReply',
  '2': const [
    const {'1': 'log_line', '3': 1, '4': 1, '5': 9, '10': 'logLine'},
  ],
};

/// Descriptor for `AuthenticateReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List authenticateReplyDescriptor = $convert.base64Decode('ChFBdXRoZW50aWNhdGVSZXBseRIZCghsb2dfbGluZRgBIAEoCVIHbG9nTGluZQ==');
