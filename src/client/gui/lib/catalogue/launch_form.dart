import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch, ImageInfo;
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ffi.dart';
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

class LaunchForm extends ConsumerWidget {
  final formKey = GlobalKey<FormState>();
  final launchRequest = LaunchRequest();
  final mountRequests = <MountRequest>[];

  LaunchForm({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final imageInfo = ref.watch(launchingImageProvider);
    final randomName = ref.watch(randomNameProvider);
    final vmNames = ref.watch(vmNamesProvider);

    final closeButton = IconButton(
      icon: const Icon(Icons.close),
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
    );

    final nameInput = SpecInput(
      autofocus: true,
      helper:
          'Names are immutable. If no names are provided, Multipass will randomly generate a name.',
      hint: randomName,
      validator: nameValidator(vmNames),
      onSaved: (value) => launchRequest.instanceName =
          value.isNullOrBlank ? randomName : value!,
      width: 360,
    );

    final chosenImageName = Text(
      imageName(imageInfo),
      style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w300),
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

    final launchButton = TextButton(
      onPressed: () {
        launchRequest.clear();
        mountRequests.clear();

        if (!formKey.currentState!.validate()) return;

        formKey.currentState!.save();
        final aliasInfo = imageInfo.aliasesInfo.first;
        launchRequest.image = aliasInfo.alias;
        if (aliasInfo.hasRemoteName()) {
          launchRequest.remoteName = aliasInfo.remoteName;
        }

        for (final mountRequest in mountRequests) {
          mountRequest.targetPaths.first.instanceName =
              launchRequest.instanceName;
        }

        final grpcClient = ref.read(grpcClientProvider);
        final operation = LaunchOperation(
          stream: grpcClient.launch(launchRequest, mountRequests),
          name: launchRequest.instanceName,
          image: imageName(imageInfo),
        );
        ref.read(launchOperationProvider.notifier).state = operation;
      },
      child: const Text('Launch'),
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
          const Text('Configure instance', style: TextStyle(fontSize: 37)),
          const SizedBox(width: 16),
          nameInput,
          const Spacer(),
          chosenImageName,
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
        bridgedSwitch,
        const SizedBox(height: 40),
        MountPointList(
          width: 300,
          onSaved: (requests) => mountRequests.addAll(requests),
        ),
        const SizedBox(height: 40),
        Row(children: [
          launchButton,
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

FormFieldValidator<String> nameValidator(Iterable<String> existingNames) {
  return (String? value) {
    if (value!.isEmpty) {
      return null;
    }
    if (value.length < 2) {
      return 'Name must be at least 2 characters\n';
    }
    if (RegExp(r'[^A-Za-z0-9\-]').hasMatch(value)) {
      return 'Name must contain only letters, numbers and dashes\n';
    }
    if (RegExp(r'^[^A-Za-z]').hasMatch(value)) {
      return 'Name must start with a letter\n';
    }
    if (RegExp(r'[^A-Za-z0-9]$').hasMatch(value)) {
      return 'Name must end in digit or letter\n';
    }
    if (existingNames.contains(value)) {
      return 'Name is already in use\n';
    }
    return null;
  };
}
