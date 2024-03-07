import 'package:flutter/material.dart' hide Switch;
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';

import '../ffi.dart';
import '../providers.dart';
import '../switch.dart';
import 'mount_points.dart';
import 'spec_input.dart';

class EditVmForm extends ConsumerStatefulWidget {
  final String name;

  const EditVmForm(this.name, {super.key});

  @override
  ConsumerState<EditVmForm> createState() => _EditVmFormState();
}

class _EditVmFormState extends ConsumerState<EditVmForm> {
  final formKey = GlobalKey<FormState>();
  final formValues = <String, String>{};
  final mountRequests = <MountRequest>[];

  late final cpusKey = 'local.${widget.name}.cpus';
  late final memoryKey = 'local.${widget.name}.memory';
  late final diskKey = 'local.${widget.name}.disk';
  late final bridgedKey = 'local.${widget.name}.bridged';

  final cpusController = TextEditingController();
  final memoryController = TextEditingController();
  final diskController = TextEditingController();

  String cpusInitial = '';
  String memoryInitial = '';
  String diskInitial = '';
  bool? bridgedInitial;
  bool hasNetworks = false;

  @override
  void initState() {
    super.initState();
    _initState();
  }

  void _initState() async {
    cpusInitial = await ref.read(daemonSettingProvider(cpusKey).future);
    cpusController.text = cpusInitial;
    memoryInitial = await ref.read(daemonSettingProvider(memoryKey).future);
    memoryController.text = memoryInitial;
    diskInitial = await ref.read(daemonSettingProvider(diskKey).future);
    diskController.text = diskInitial;
    bridgedInitial = (await ref.read(daemonSettingProvider(bridgedKey).future))
        .toBoolOption
        .toNullable();
    hasNetworks = await ref
        .watch(grpcClientProvider)
        .networks()
        .then((n) => n.isNotEmpty)
        .onError((_, __) => false);
    final mounts = ref.read(vmInfoProvider(widget.name)).mountInfo.mountPaths;
    setState(() {
      for (final mount in mounts) {
        final request = MountRequest(
          mountMaps: mount.mountMaps,
          sourcePath: mount.sourcePath,
          targetPaths: [
            TargetPathInfo(
              instanceName: widget.name,
              targetPath: mount.targetPath,
            ),
          ],
        );
        mountRequests.add(request);
      }
    });
  }

  @override
  void dispose() {
    cpusController.dispose();
    memoryController.dispose();
    diskController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final closeButton = IconButton(
      icon: const Icon(Icons.close),
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
    );

    final cpusInput = SpecInput(
      controller: cpusController,
      validator: (value) => (int.tryParse(value!) ?? 0) > 0
          ? null
          : 'Number of CPUs must be greater than 0',
      onSaved: (value) => formValues[cpusKey] = value!,
      inputFormatters: [FilteringTextInputFormatter.digitsOnly],
      helper: 'Number of cores',
      label: 'CPUs',
    );

    final memoryInput = SpecInput(
      controller: memoryController,
      helper: 'Default unit in Gigabytes',
      label: 'Memory',
      validator: memorySizeValidator,
      onSaved: (value) => formValues[memoryKey] =
          double.tryParse(value!) != null ? '${value}GB' : value,
    );

    final diskInput = SpecInput(
      controller: diskController,
      helper: 'Default unit in Gigabytes',
      label: 'Disk',
      validator: (value) {
        value = double.tryParse(value!) != null ? '${value}GB' : value;
        try {
          final givenValue = memoryInBytes(value);
          final currentValue = memoryInBytes(diskInitial);
          return givenValue < currentValue ? 'Disk can only grow' : null;
        } catch (_) {
          return 'Invalid memory size';
        }
      },
      onSaved: (value) => formValues[diskKey] =
          double.tryParse(value!) != null ? '${value}GB' : value,
    );

    final bridgedSwitch = FormField<bool>(
      initialValue: bridgedInitial ?? false,
      onSaved: (value) => formValues[bridgedKey] = '${value!}',
      builder: (field) => Switch(
        label: 'Connect to bridged network',
        value: field.value!,
        onChanged: field.didChange,
      ),
    );

    final saveButton = TextButton(
      onPressed: () async {
        formValues.clear();
        mountRequests.clear();

        if (!formKey.currentState!.validate()) return;

        formKey.currentState!.save();

        final grpcClient = ref.read(grpcClientProvider);
        await grpcClient.umount(widget.name);
        for (final mountRequest in mountRequests) {
          mountRequest.targetPaths.first.instanceName = widget.name;
          await grpcClient.mount(mountRequest);
        }

        final keys = [cpusKey, memoryKey, diskKey, if (hasNetworks) bridgedKey];
        for (final key in keys) {
          ref.read(daemonSettingProvider(key).notifier).set(formValues[key]!);
        }

        if (mounted) Scaffold.of(context).closeEndDrawer();
      },
      child: const Text('Save'),
    );

    final cancelButton = OutlinedButton(
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
      child: const Text('Cancel'),
    );

    final formBody = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(children: [const Spacer(), closeButton]),
        Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(widget.name, style: const TextStyle(fontSize: 37)),
          const SizedBox(width: 16),
          const Spacer(),
          const SizedBox(width: 16),
        ]),
        const Divider(),
        Row(children: [
          cpusInput,
          const SizedBox(width: 24),
          memoryInput,
          const SizedBox(width: 24),
          diskInput,
        ]),
        const SizedBox(height: 40),
        if (hasNetworks) bridgedSwitch,
        const SizedBox(height: 40),
        MountPointList(mountRequests: mountRequests),
        const SizedBox(height: 40),
        Row(children: [
          saveButton,
          const SizedBox(width: 16),
          cancelButton,
        ]),
      ],
    );

    return Container(
      color: Colors.white,
      padding: const EdgeInsets.all(16),
      alignment: Alignment.topCenter,
      child: Form(
        key: formKey,
        autovalidateMode: AutovalidateMode.always,
        child: SingleChildScrollView(child: formBody),
      ),
    );
  }
}
