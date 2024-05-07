import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';
import 'package:intl/intl.dart';

import '../extensions.dart';
import '../ffi.dart';
import '../notifications.dart';
import '../providers.dart';
import 'cpu_sparkline.dart';
import 'memory_usage.dart';
import 'mount_points.dart';
import 'spec_input.dart';
import 'terminal_tabs.dart';
import 'vm_action_buttons.dart';
import 'vm_status_icon.dart';

enum VmDetailsLocation { shells, details }

final vmScreenLocationProvider =
    StateProvider.autoDispose.family<VmDetailsLocation, String>((_, __) {
  return VmDetailsLocation.shells;
});

class VmDetailsScreen extends ConsumerWidget {
  final String name;

  const VmDetailsScreen(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final location = ref.watch(vmScreenLocationProvider(name));

    return Scaffold(
      body: Column(children: [
        VmDetailsHeader(name),
        Expanded(
          child: Stack(fit: StackFit.expand, children: [
            Visibility(
              visible: location == VmDetailsLocation.shells,
              maintainState: true,
              child: TerminalTabs(name),
            ),
            Visibility(
              visible: location == VmDetailsLocation.details,
              child: VmDetails(name),
            ),
          ]),
        ),
      ]),
    );
  }
}

class VmDetailsHeader extends ConsumerWidget {
  final String name;

  const VmDetailsHeader(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(vmInfoProvider(name));

    final cpu = VmStat(
      width: 120,
      height: 35,
      label: 'CPU USAGE',
      child: CpuSparkline(info.name),
    );

    final memory = VmStat(
      width: 110,
      height: 35,
      label: 'MEMORY USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.memoryUsage,
        total: info.memoryTotal,
      ),
    );

    final disk = VmStat(
      width: 110,
      height: 35,
      label: 'DISK USAGE',
      child: MemoryUsage(
        used: info.instanceInfo.diskUsage,
        total: info.diskTotal,
      ),
    );

    final currentLocation = ref.watch(vmScreenLocationProvider(name));
    final buttonStyle = Theme.of(context).outlinedButtonTheme.style;

    OutlinedButton locationButton(VmDetailsLocation location) {
      final style = buttonStyle?.copyWith(
        shape: const MaterialStatePropertyAll(RoundedRectangleBorder()),
        backgroundColor: location == currentLocation
            ? const MaterialStatePropertyAll(Color(0xff333333))
            : null,
        foregroundColor: location == currentLocation
            ? const MaterialStatePropertyAll(Colors.white)
            : null,
      );
      return OutlinedButton(
        style: style,
        child: Text(location.name.capitalize()),
        onPressed: () {
          ref.read(vmScreenLocationProvider(name).notifier).state = location;
        },
      );
    }

    final locationButtons = Row(mainAxisSize: MainAxisSize.min, children: [
      locationButton(VmDetailsLocation.shells),
      locationButton(VmDetailsLocation.details),
    ]);

    final list = [
      Expanded(
        child: Text(
          name.nonBreaking,
          style: const TextStyle(fontSize: 24, fontWeight: FontWeight.w300),
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
        ),
      ),
      locationButtons,
      cpu,
      memory,
      disk,
      VmActionButtons(name),
    ];

    return Padding(
      padding: const EdgeInsets.all(20),
      child: Row(children: list.gap(width: 40).toList()),
    );
  }
}

class VmDetails extends ConsumerWidget {
  final String name;

  const VmDetails(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return SingleChildScrollView(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          GeneralDetails(name),
          const Divider(height: 60),
          ResourcesDetails(name),
          const Divider(height: 60),
          BridgedDetails(name),
          const Divider(height: 60),
          MountDetails(name),
        ]),
      ),
    );
  }
}

class GeneralDetails extends ConsumerWidget {
  final String name;

  const GeneralDetails(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(vmInfoProvider(name));

    final status = VmStat(
      width: 100,
      height: 50,
      label: 'STATE',
      child: VmStatusIcon(info.instanceStatus.status),
    );

    final image = VmStat(
      width: 150,
      height: 50,
      label: 'IMAGE',
      child: Text(info.instanceInfo.currentRelease),
    );

    final privateIp = VmStat(
      width: 150,
      height: 50,
      label: 'PRIVATE IP',
      child: Text(info.instanceInfo.ipv4.firstOrNull ?? '-'),
    );

    final publicIp = VmStat(
      width: 150,
      height: 50,
      label: 'PUBLIC IP',
      child: Text(info.instanceInfo.ipv4.skip(1).firstOrNull ?? '-'),
    );

    final created = VmStat(
      width: 140,
      height: 50,
      label: 'CREATED',
      child: Text(
        DateFormat('yyyy-MM-dd HH:mm:ss')
            .format(info.instanceInfo.creationTimestamp.toDateTime()),
      ),
    );

    final uptime = VmStat(
      width: 300,
      height: 50,
      label: 'UPTIME',
      child: Text(info.instanceInfo.uptime),
    );

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(
          height: 50,
          child: Text('General', style: TextStyle(fontSize: 24)),
        ),
        Wrap(spacing: 50, runSpacing: 25, children: [
          status,
          image,
          privateIp,
          publicIp,
          created,
          uptime,
        ]),
      ],
    );
  }
}

class ResourcesDetails extends ConsumerStatefulWidget {
  final String name;

  const ResourcesDetails(this.name, {super.key});

  @override
  ConsumerState<ResourcesDetails> createState() => _ResourcesDetailsState();
}

class _ResourcesDetailsState extends ConsumerState<ResourcesDetails> {
  final formKey = GlobalKey<FormState>();
  bool editing = false;

  late final cpusProvider = daemonSettingProvider('local.${widget.name}.cpus');
  late final memoryProvider =
      daemonSettingProvider('local.${widget.name}.memory');
  late final diskProvider = daemonSettingProvider('local.${widget.name}.disk');

  @override
  Widget build(BuildContext context) {
    final cpus = ref.watch(cpusProvider).valueOrNull;
    final memory = ref.watch(memoryProvider).valueOrNull;
    final disk = ref.watch(diskProvider).valueOrNull;
    final stopped = ref.watch(vmInfoProvider(widget.name).select((info) {
      return info.instanceStatus.status == Status.STOPPED;
    }));

    if (!stopped) editing = false;

    final cpusInput = SpecInput(
      key: Key('cpus-$cpus'),
      label: 'CPUs',
      initialValue: cpus,
      helper: editing ? 'Number of cores' : null,
      enabled: editing,
      inputFormatters: [FilteringTextInputFormatter.digitsOnly],
      validator: cpus != null
          ? (value) => (int.tryParse(value!) ?? 0) > 0
              ? null
              : 'Number of CPUs must be greater than 0'
          : null,
      onSaved: (value) {
        if (value == cpus) return;
        ref
            .read(cpusProvider.notifier)
            .set(value!)
            .onError(ref.notifyError((error) => 'Failed to set CPUs: $error'));
      },
    );

    final memoryInput = SpecInput(
      key: Key('memory-$memory'),
      label: 'Memory',
      initialValue: memory,
      helper: editing ? 'Default unit in Gigabytes' : null,
      enabled: editing,
      validator: memory != null ? memorySizeValidator : null,
      onSaved: (value) {
        if (value == memory) return;
        ref
            .read(memoryProvider.notifier)
            .set(double.tryParse(value!) != null ? '${value}GB' : value)
            .onError(ref.notifyError((e) => 'Failed to set memory size: $e'));
      },
    );

    final diskInput = SpecInput(
      key: Key('disk-$disk'),
      label: 'Disk',
      initialValue: disk,
      helper: editing ? 'Default unit in Gigabytes' : null,
      enabled: editing,
      validator: disk != null
          ? (value) {
              value = double.tryParse(value!) != null ? '${value}GB' : value;
              try {
                final givenValue = memoryInBytes(value);
                final currentValue = memoryInBytes(disk);
                return givenValue < currentValue ? 'Disk can only grow' : null;
              } catch (_) {
                return 'Invalid memory size';
              }
            }
          : null,
      onSaved: (value) {
        if (value == disk) return;
        ref
            .read(diskProvider.notifier)
            .set(double.tryParse(value!) != null ? '${value}GB' : value)
            .onError(ref.notifyError((e) => 'Failed to set disk size: $e'));
      },
    );

    final saveButton = TextButton(
      onPressed: () {
        if (!formKey.currentState!.validate()) return;
        formKey.currentState!.save();
        setState(() => editing = false);
      },
      child: const Text('Save changes'),
    );

    final configureButton = TooltipVisibility(
      visible: !stopped,
      child: Tooltip(
        message: 'Stop instance to configure',
        child: OutlinedButton(
          onPressed: stopped ? () => setState(() => editing = true) : null,
          child: const Text('Configure'),
        ),
      ),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        formKey.currentState?.reset();
        setState(() => editing = false);
      },
      child: const Text('Cancel'),
    );

    return Form(
      key: formKey,
      autovalidateMode: AutovalidateMode.always,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(children: [
            const SizedBox(
              height: 50,
              child: Text('Resources', style: TextStyle(fontSize: 24)),
            ),
            const Spacer(),
            editing ? cancelButton : configureButton,
          ]),
          Wrap(spacing: 50, runSpacing: 25, children: [
            cpusInput,
            memoryInput,
            diskInput,
          ]),
          if (editing)
            Padding(
              padding: const EdgeInsets.only(top: 16),
              child: saveButton,
            ),
        ],
      ),
    );
  }
}

class BridgedDetails extends ConsumerStatefulWidget {
  final String name;

  const BridgedDetails(this.name, {super.key});

  @override
  ConsumerState<BridgedDetails> createState() => _BridgedDetailsState();
}

class _BridgedDetailsState extends ConsumerState<BridgedDetails> {
  final formKey = GlobalKey<FormState>();
  bool editing = false;

  final bridgedNetworkProvider = daemonSettingProvider('local.bridged-network');
  late final bridgedProvider =
      daemonSettingProvider('local.${widget.name}.bridged');

  @override
  Widget build(BuildContext context) {
    final networks = ref.watch(networksProvider);
    final bridgedNetworkSetting = ref.watch(bridgedNetworkProvider).valueOrNull;
    final bridged = ref.watch(bridgedProvider.select((value) {
      return value.valueOrNull?.toBoolOption.toNullable();
    }));
    final stopped = ref.watch(vmInfoProvider(widget.name).select((info) {
      return info.instanceStatus.status == Status.STOPPED;
    }));

    if (!stopped) editing = false;

    final bridgedCheckbox = FormField<bool>(
      key: Key('bridged-$bridged'),
      initialValue: bridged ?? false,
      onSaved: (value) {
        if (value!) {
          ref.read(bridgedProvider.notifier).set(value.toString()).onError(
              ref.notifyError((e) => 'Failed to set bridged network: $e'));
        }
        setState(() => editing = false);
      },
      builder: (field) {
        final validBridgedNetwork = networks.contains(bridgedNetworkSetting);
        return CheckboxListTile(
          contentPadding: EdgeInsets.zero,
          controlAffinity: ListTileControlAffinity.leading,
          enabled: validBridgedNetwork,
          onChanged: field.didChange,
          title: const Text('Connect to bridged network.'),
          value: field.value!,
          visualDensity: VisualDensity.standard,
          subtitle: Text(
            networks.isEmpty
                ? 'No networks found.'
                : validBridgedNetwork
                    ? "Once connection is established, you won't be able to unset it."
                    : 'No valid bridged network is set.',
          ),
        );
      },
    );

    final saveButton = TextButton(
      onPressed: () => formKey.currentState?.save(),
      child: const Text('Save'),
    );

    final configureButton = TooltipVisibility(
      visible: !stopped,
      child: Tooltip(
        message: 'Stop instance to configure',
        child: OutlinedButton(
          onPressed: stopped ? () => setState(() => editing = true) : null,
          child: const Text('Configure'),
        ),
      ),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        formKey.currentState?.reset();
        setState(() => editing = false);
      },
      child: const Text('Cancel'),
    );

    return Form(
      key: formKey,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(children: [
            const SizedBox(
              height: 50,
              child: Text('Bridged network', style: TextStyle(fontSize: 24)),
            ),
            const Spacer(),
            if (editing)
              cancelButton
            else if (!(bridged ?? false))
              configureButton,
          ]),
          editing
              ? SizedBox(width: 300, child: bridgedCheckbox)
              : Text(
                  'Status: ${bridged ?? false ? '' : 'not'} connected',
                  style: const TextStyle(fontSize: 16),
                ),
          if (editing)
            Padding(
              padding: const EdgeInsets.only(top: 16),
              child: saveButton,
            ),
        ],
      ),
    );
  }
}

class MountDetails extends ConsumerStatefulWidget {
  final String name;

  const MountDetails(this.name, {super.key});

  @override
  ConsumerState<MountDetails> createState() => _MountDetailsState();
}

class _MountDetailsState extends ConsumerState<MountDetails> {
  final formKey = GlobalKey<FormState>();
  bool editing = false;
  final mountRequests = <MountRequest>[];

  @override
  Widget build(BuildContext context) {
    final mounts = ref.watch(vmInfoProvider(widget.name).select((info) {
      return info.mountInfo.mountPaths.build();
    }));
    final stopped = ref.watch(vmInfoProvider(widget.name).select((info) {
      return info.instanceStatus.status == Status.STOPPED;
    }));

    if (!stopped) editing = false;

    final viewMountPoints = Column(children: [
      if (mounts.isNotEmpty)
        const DefaultTextStyle(
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.bold,
            color: Colors.black,
          ),
          child: Row(children: [
            Expanded(child: Text('SOURCE PATH')),
            Expanded(child: Text('TARGET PATH')),
          ]),
        ),
      for (final mount in mounts) ...[
        const Divider(height: 20),
        Row(children: [
          Expanded(
            child: Text(mount.sourcePath, style: const TextStyle(fontSize: 16)),
          ),
          Expanded(
            child: Text(mount.targetPath, style: const TextStyle(fontSize: 16)),
          ),
        ])
      ],
    ]);

    final initialMountRequests = mounts.map((mount) {
      return MountRequest(
        mountMaps: mount.mountMaps,
        sourcePath: mount.sourcePath,
        targetPaths: [
          TargetPathInfo(
            instanceName: widget.name,
            targetPath: mount.targetPath,
          ),
        ],
      );
    });

    final editableMountPoints = MountPointList(
      initialMountRequests: initialMountRequests.toBuiltList(),
      onSaved: (newMountRequests) => mountRequests.addAll(newMountRequests),
    );

    final saveButton = TextButton(
      onPressed: () async {
        mountRequests.clear();
        formKey.currentState?.save();
        setState(() => editing = false);
        final grpcClient = ref.read(grpcClientProvider);
        await grpcClient.umount(widget.name);
        for (final mountRequest in mountRequests) {
          mountRequest.targetPaths.first.instanceName = widget.name;
          await grpcClient.mount(mountRequest);
        }
      },
      child: const Text('Save'),
    );

    final configureButton = TooltipVisibility(
      visible: !stopped,
      child: Tooltip(
        message: 'Stop instance to configure',
        child: OutlinedButton(
          onPressed: stopped ? () => setState(() => editing = true) : null,
          child: const Text('Configure'),
        ),
      ),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        formKey.currentState?.reset();
        setState(() => editing = false);
      },
      child: const Text('Cancel'),
    );

    return Form(
      key: formKey,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(children: [
            const SizedBox(
              height: 50,
              child: Text('Mounts', style: TextStyle(fontSize: 24)),
            ),
            const Spacer(),
            editing ? cancelButton : configureButton,
          ]),
          editing ? editableMountPoints : viewMountPoints,
          if (editing)
            Padding(
              padding: const EdgeInsets.only(top: 16),
              child: saveButton,
            ),
        ],
      ),
    );
  }
}

class VmStat extends StatelessWidget {
  final String label;
  final Widget child;
  final double width;
  final double height;

  const VmStat({
    super.key,
    required this.label,
    required this.child,
    required this.width,
    required this.height,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: width,
      height: height,
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(
          label,
          style: const TextStyle(fontSize: 10, fontWeight: FontWeight.bold),
        ),
        Expanded(child: Align(alignment: Alignment.centerLeft, child: child)),
      ]),
    );
  }
}
