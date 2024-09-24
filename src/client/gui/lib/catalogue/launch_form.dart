import 'dart:async';

import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch, ImageInfo;
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ffi.dart';
import '../platform/platform.dart';
import '../providers.dart';
import '../switch.dart';
import '../vm_details/mount_points.dart';
import '../vm_details/spec_input.dart';
import 'launch_panel.dart';

final launchingImageProvider = StateProvider<ImageInfo>((_) => ImageInfo());

final randomNameProvider = Provider.autoDispose(
  (ref) => generatePetname(ref.watch(vmNamesProvider)),
);

String imageName(ImageInfo imageInfo) {
  final result = '${imageInfo.os} ${imageInfo.release}';
  return imageInfo.aliasesInfo.any((a) => a.alias.contains('core'))
      ? result
      : '$result ${imageInfo.codename}';
}

class LaunchForm extends ConsumerStatefulWidget {
  const LaunchForm({super.key});

  @override
  ConsumerState<LaunchForm> createState() => _LaunchFormState();
}

class _LaunchFormState extends ConsumerState<LaunchForm> {
  final formKey = GlobalKey<FormState>();
  final mountFormKey = GlobalKey<FormState>();
  final launchRequest = LaunchRequest();
  final mountRequests = <MountRequest>[];
  var addingMount = false;
  final scrollController = ScrollController();

  @override
  void dispose() {
    scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final imageInfo = ref.watch(launchingImageProvider);
    final randomName = ref.watch(randomNameProvider);
    final vmNames = ref.watch(vmNamesProvider);

    final closeButton = IconButton(
      icon: const Icon(Icons.close),
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
    );

    final nameInput = SpecInput(
      label: 'Name',
      autofocus: true,
      helper: 'Namess cannot be changed one an instance is created',
      hint: randomName,
      validator: nameValidator(vmNames),
      onSaved: (value) => launchRequest.instanceName =
          value.isNullOrBlank ? randomName : value!,
      width: 360,
    );

    final chosenImageName = Text(
      imageName(imageInfo),
      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w300),
    );

    final cpusInput = SpecInput(
      initialValue: '1',
      validator: (value) => (int.tryParse(value!) ?? 0) > 0
          ? null
          : 'Number of CPUs must be greater than 0',
      onSaved: (value) => launchRequest.numCores = int.parse(value!),
      inputFormatters: [FilteringTextInputFormatter.digitsOnly],
      helper: 'Number of cores',
      label: 'CPUs',
    );

    final memoryInput = SpecInput(
      initialValue: '1',
      helper: 'Default unit in Gigabytes',
      label: 'Memory',
      validator: memorySizeValidator,
      onSaved: (value) => launchRequest.memSize =
          double.tryParse(value!) != null ? '${value}GB' : value,
    );

    final diskInput = SpecInput(
      initialValue: '5',
      helper: 'Default unit in Gigabytes',
      label: 'Disk',
      validator: memorySizeValidator,
      onSaved: (value) => launchRequest.diskSpace =
          double.tryParse(value!) != null ? '${value}GB' : value,
    );

    final bridgedSwitch = FormField<bool>(
      initialValue: false,
      onSaved: (value) {
        if (value!) {
          launchRequest.networkOptions
              .add(LaunchRequest_NetworkOptions(id: 'bridged'));
        }
      },
      builder: (field) => Switch(
        label: 'Connect to bridged network',
        value: field.value!,
        onChanged: field.didChange,
      ),
    );

    final mountPointsView = MountPointsView(
      allowDelete: true,
      mounts: mountRequests.map((r) => MountPaths(
            sourcePath: r.sourcePath,
            targetPath: r.targetPaths.first.targetPath,
          )),
      onDelete: (mountPaths) => setState(() {
        mountRequests.removeWhere((r) =>
            r.sourcePath == mountPaths.sourcePath &&
            r.targetPaths.first.targetPath == mountPaths.targetPath);
      }),
    );

    final addMountButton = OutlinedButton(
      onPressed: () => setState(() {
        addingMount = true;
        Timer(100.milliseconds, () {
          scrollController.animateTo(
            scrollController.position.maxScrollExtent,
            duration: 200.milliseconds,
            curve: Curves.ease,
          );
        });
      }),
      child: const Text('Add mount'),
    );

    final saveMountButton = TextButton(
      onPressed: () {
        final mountFormState = mountFormKey.currentState;
        if (mountFormState == null) return;
        if (!mountFormState.validate()) return;
        mountFormState.save();
      },
      child: const Text('Save'),
    );

    final cancelMountButton = OutlinedButton(
      onPressed: () => setState(() => addingMount = false),
      child: const Text('Cancel'),
    );

    final editableMountPoint = EditableMountPoint(
      existingTargets: mountRequests.map((r) => r.targetPaths.first.targetPath),
      initialSource:
          mountRequests.any((r) => r.sourcePath == mpPlatform.homeDirectory)
              ? null
              : mpPlatform.homeDirectory,
      onSaved: (request) => setState(() {
        mountRequests.add(request);
        addingMount = false;
      }),
    );

    final mountForm = Form(
      key: mountFormKey,
      child: Column(children: [
        editableMountPoint,
        const SizedBox(height: 16),
        Row(children: [
          saveMountButton,
          const SizedBox(width: 16),
          cancelMountButton,
        ]),
      ]),
    );

    final formBody = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(children: [
          const Text('Configure instance', style: TextStyle(fontSize: 24)),
          const Spacer(),
          closeButton,
        ]),
        const SizedBox(height: 20),
        const Text(
          'Image',
          style: TextStyle(fontSize: 18),
        ),
        const SizedBox(height: 4),
        chosenImageName,
        const SizedBox(height: 16),
        Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
          nameInput,
          const Spacer(),
        ]),
        const Divider(height: 60),
        const SizedBox(
          height: 50,
          child: Text('Resources', style: TextStyle(fontSize: 24)),
        ),
        Row(children: [
          cpusInput,
          const SizedBox(width: 24),
          memoryInput,
          const SizedBox(width: 24),
          diskInput,
        ]),
        const Divider(height: 60),
        const SizedBox(
          height: 50,
          child: Text('Bridged network', style: TextStyle(fontSize: 24)),
        ),
        bridgedSwitch,
        const Divider(height: 60),
        const SizedBox(
          height: 50,
          child: Text('Mounts', style: TextStyle(fontSize: 24)),
        ),
        mountPointsView,
        if (mountRequests.isNotEmpty) const SizedBox(height: 20),
        addingMount ? mountForm : addMountButton,
      ],
    );

    final launchButton = TextButton(
      onPressed: () => launch(imageInfo),
      child: const Text('Launch'),
    );

    final cancelButton = OutlinedButton(
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
      child: const Text('Cancel'),
    );

    return Stack(fit: StackFit.loose, children: [
      Positioned.fill(
        bottom: 80,
        child: Container(
          alignment: Alignment.topCenter,
          color: Colors.white,
          padding: const EdgeInsets.all(16),
          child: Form(
            key: formKey,
            autovalidateMode: AutovalidateMode.always,
            child: SingleChildScrollView(
              controller: scrollController,
              child: formBody,
            ),
          ),
        ),
      ),
      Align(
        alignment: Alignment.bottomCenter,
        child: Container(
          color: Colors.white,
          padding: const EdgeInsets.all(16).copyWith(top: 4),
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            const Divider(height: 30),
            Row(children: [
              launchButton,
              const SizedBox(width: 16),
              cancelButton,
            ]),
          ]),
        ),
      ),
    ]);
  }

  void launch(ImageInfo imageInfo) {
    launchRequest.clear();

    final formState = formKey.currentState;
    if (formState == null) return;
    final mountFormState = mountFormKey.currentState;

    if (!formState.validate()) return;
    if (!(mountFormState?.validate() ?? true)) return;

    mountFormState?.save();
    formState.save();

    final aliasInfo = imageInfo.aliasesInfo.first;
    launchRequest.image = aliasInfo.alias;
    if (aliasInfo.hasRemoteName()) {
      launchRequest.remoteName = aliasInfo.remoteName;
    }

    for (final mountRequest in mountRequests) {
      mountRequest.targetPaths.first.instanceName = launchRequest.instanceName;
    }

    final grpcClient = ref.read(grpcClientProvider);
    final operation = LaunchOperation(
      stream: grpcClient.launch(launchRequest, mountRequests),
      name: launchRequest.instanceName,
      image: imageName(imageInfo),
    );
    ref.read(launchOperationProvider.notifier).state = operation;
  }
}

FormFieldValidator<String> nameValidator(Iterable<String> existingNames) {
  return (String? value) {
    if (value!.isEmpty) {
      return null;
    }
    if (value.length < 2) {
      return 'Name must be at least 2 characters';
    }
    if (RegExp(r'[^A-Za-z0-9\-]').hasMatch(value)) {
      return 'Name must contain only letters, numbers and dashes';
    }
    if (RegExp(r'^[^A-Za-z]').hasMatch(value)) {
      return 'Name must start with a letter';
    }
    if (RegExp(r'[^A-Za-z0-9]$').hasMatch(value)) {
      return 'Name must end in digit or letter';
    }
    if (existingNames.contains(value)) {
      return 'Name is already in use';
    }
    return null;
  };
}
