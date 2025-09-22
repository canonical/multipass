import 'dart:async';

import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch, ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:protobuf/protobuf.dart';
import 'package:rxdart/rxdart.dart';

import '../dropdown.dart';
import '../ffi.dart';
import '../notifications.dart';
import '../platform/platform.dart';
import '../providers.dart';
import '../sidebar.dart';
import '../switch.dart';
import '../vm_details/cpus_slider.dart';
import '../vm_details/disk_slider.dart';
import '../vm_details/mapping_slider.dart';
import '../vm_details/mount_points.dart';
import '../vm_details/ram_slider.dart';
import '../vm_details/spec_input.dart';

final launchingImageProvider = StateProvider<ImageInfo>((_) => ImageInfo());

final randomNameProvider = Provider.autoDispose(
  (ref) => generatePetname(ref.watch(vmNamesProvider)),
);

String imageName(ImageInfo imageInfo) {
  final result = '${imageInfo.os} ${imageInfo.release}';
  return imageInfo.aliasesInfo
          .any((a) => RegExp(r'\bcore\d{0,2}\b').hasMatch(a.alias))
      ? result
      : '$result ${imageInfo.codename}';
}

final defaultCpus = 1;
final defaultRam = 1.gibi;
final defaultDisk = 5.gibi;

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

  final bridgedNetworkProvider = daemonSettingProvider('local.bridged-network');

  @override
  Widget build(BuildContext context) {
    final imageInfo = ref.watch(launchingImageProvider);
    final randomName = ref.watch(randomNameProvider);
    final vmNames = ref.watch(vmNamesProvider);
    final deletedVms = ref.watch(deletedVmsProvider);
    final networks = ref.watch(networksProvider);
    final bridgedNetworkSetting = ref.watch(bridgedNetworkProvider).valueOrNull;

    final closeButton = IconButton(
      icon: const Icon(Icons.close),
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
    );

    final nameInput = SpecInput(
      label: 'Name',
      autofocus: true,
      helper: 'Names cannot be changed once an instance is created',
      hint: randomName,
      validator: nameValidator(vmNames, deletedVms),
      onSaved: (value) => launchRequest.instanceName =
          value.isNullOrBlank ? randomName : value!,
      width: 360,
    );

    final zonesAsync = ref.watch(zonesProvider);
    final zoneDropdown = FormField<String>(
      initialValue: 'auto',
      onSaved: (value) =>
          launchRequest.zone = value == 'auto' ? '' : value ?? '',
      builder: (field) {
        return Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Text('Pseudo zone'),
                const SizedBox(width: 4),
                Tooltip(
                  message:
                      'Pseudo zones simulate availability zones in public clouds\nand allow for testing resilience against real-world incidents',
                  child: Icon(Icons.info_outline,
                      size: 16, color: Colors.blue[700]),
                ),
              ],
            ),
            const SizedBox(height: 8),
            zonesAsync.when(
              loading: () => const SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(),
              ),
              error: (_, __) => const Text('Error loading zones'),
              data: (zones) {
                final availableZones = zones.where((z) => z.available).toList();
                final hasAvailableZones = availableZones.isNotEmpty;

                return Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Dropdown<String>(
                      value: field.value,
                      onChanged: field.didChange,
                      items: {
                        'auto': 'Auto (assign automatically)',
                        for (final zone in availableZones) ...<String, String>{
                          zone.name: 'Zone ${zone.name}',
                        },
                      },
                    ),
                    if (!hasAvailableZones)
                      const Padding(
                        padding: EdgeInsets.only(top: 8),
                        child: Text(
                          'Cannot launch new instances - all zones are unavailable',
                          style: TextStyle(color: Colors.red),
                        ),
                      ),
                  ],
                );
              },
            ),
          ],
        );
      },
    );

    final chosenImageName = Text(
      imageName(imageInfo),
      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w300),
    );

    final cpusSlider = CpusSlider(
      initialValue: defaultCpus,
      onSaved: (value) => launchRequest.numCores = value!,
    );

    // Determine minimums based on image type
    final isCore = imageInfo.aliasesInfo
        .any((a) => RegExp(r'\bcore\d{0,2}\b').hasMatch(a.alias));
    final minRam = isCore ? 512.mebi : 1024.mebi;
    final minDisk = isCore ? 1.gibi : 5.gibi;

    final memorySlider = RamSlider(
      initialValue: defaultRam,
      min: minRam,
      onSaved: (value) => launchRequest.memSize = '${value!}B',
    );

    final diskSlider = DiskSlider(
      initialValue: defaultDisk,
      min: minDisk,
      onSaved: (value) => launchRequest.diskSpace = '${value!}B',
    );

    final validBridgedNetwork = networks.contains(bridgedNetworkSetting);
    final bridgedSwitch = FormField<bool>(
      enabled: validBridgedNetwork,
      initialValue: false,
      onSaved: (value) {
        if (value!) {
          launchRequest.networkOptions.add(
            LaunchRequest_NetworkOptions(id: 'bridged'),
          );
        }
      },
      builder: (field) {
        final message = networks.isEmpty
            ? 'No networks found.'
            : validBridgedNetwork
                ? "Connect to the bridged network.\nOnce established, you won't be able to unset the connection."
                : 'No valid bridged network is set.\nYou can set one in the Settings page.';

        return Switch(
          label: message,
          value: validBridgedNetwork ? field.value! : false,
          enabled: validBridgedNetwork,
          onChanged: field.didChange,
        );
      },
    );

    final mountPointsView = MountPointsView(
      allowDelete: true,
      mounts: mountRequests.map(
        (r) => MountPaths(
          sourcePath: r.sourcePath,
          targetPath: r.targetPaths.first.targetPath,
        ),
      ),
      onDelete: (mountPaths) => setState(() {
        mountRequests.removeWhere(
          (r) =>
              r.sourcePath == mountPaths.sourcePath &&
              r.targetPaths.first.targetPath == mountPaths.targetPath,
        );
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
      child: Column(
        children: [
          editableMountPoint,
          const SizedBox(height: 16),
          Row(
            children: [
              saveMountButton,
              const SizedBox(width: 16),
              cancelMountButton,
            ],
          ),
        ],
      ),
    );

    final formBody = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Text('Configure instance', style: TextStyle(fontSize: 24)),
            const Spacer(),
            closeButton,
          ],
        ),
        const SizedBox(height: 20),
        const Text('Image', style: TextStyle(fontSize: 18)),
        const SizedBox(height: 4),
        chosenImageName,
        const SizedBox(height: 16),
        Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
          nameInput,
          const SizedBox(width: 32),
          zoneDropdown,
          const Spacer(),
        ]),
        const Divider(height: 60),
        const SizedBox(
          height: 50,
          child: Text('Resources', style: TextStyle(fontSize: 24)),
        ),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(child: cpusSlider),
            const SizedBox(width: 86),
            Expanded(child: memorySlider),
            const SizedBox(width: 86),
            Expanded(child: diskSlider),
          ],
        ),
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

    final availableZones =
        zonesAsync.value?.where((z) => z.available).toList() ?? [];
    final hasAvailableZones = availableZones.isNotEmpty;

    final launchButton = TextButton(
      onPressed: hasAvailableZones ? () => launch(imageInfo) : null,
      child: const Text('Launch'),
    );

    final launchAndConfigureNextButton = OutlinedButton(
      onPressed: () => launch(imageInfo, configureNext: true),
      child: const Text('Launch & Configure next'),
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
          child: Form(
            key: formKey,
            autovalidateMode: AutovalidateMode.always,
            child: SingleChildScrollView(
              clipBehavior: Clip.none,
              controller: scrollController,
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: formBody,
              ),
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
              launchAndConfigureNextButton,
              const SizedBox(width: 16),
              launchAndConfigureNextButton,
              const SizedBox(width: 16),
              cancelButton,
            ]),
          ]),
        ),
      ),
    ]);
  }

  void launch(ImageInfo imageInfo, {bool configureNext = false}) {
  void launch(ImageInfo imageInfo, {bool configureNext = false}) {
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

    initiateLaunchFlow(
      ref,
      launchRequest.deepCopy(),
      mountRequests.map((r) => r.deepCopy()).toList(),
    );

    if (!configureNext) {
      Scaffold.of(context).closeEndDrawer();
      ref
          .read(sidebarKeyProvider.notifier)
          .set('vm-${launchRequest.instanceName}');
    }
  }
}

void initiateLaunchFlow(
  WidgetRef ref,
  LaunchRequest launchRequest, [
  List<MountRequest> mountRequests = const [],
]) {
  final grpcClient = ref.read(grpcClientProvider);
  final launchingVmsNotifier = ref.read(launchingVmsProvider.notifier);

  launchingVmsNotifier.add(launchRequest);
  final cancelCompleter = Completer<void>();
  final launchStream = grpcClient
      .launch(
        launchRequest,
        mountRequests: mountRequests,
        cancel: cancelCompleter.future,
      )
      .doOnDone(() => launchingVmsNotifier.remove(launchRequest.instanceName));

  final notification = LaunchingNotification(
    name: launchRequest.instanceName,
    cancelCompleter: cancelCompleter,
    stream: launchStream,
  );

  ref.read(notificationsProvider.notifier).add(notification);
}

FormFieldValidator<String> nameValidator(
  Iterable<String> existingNames,
  Iterable<String> deletedNames,
) {
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
    if (deletedNames.contains(value)) {
      return 'Name is already in use by a deleted instance';
    }
    return null;
  };
}
