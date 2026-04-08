import 'dart:async';

import 'package:basics/basics.dart';
import 'package:flutter/material.dart' hide Switch, ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:rxdart/rxdart.dart';

import '../ffi.dart';
import '../l10n/app_localizations.dart';
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

class LaunchingImageNotifier extends Notifier<ImageInfo> {
  @override
  ImageInfo build() {
    return ImageInfo();
  }

  void set(ImageInfo imageInfo) {
    state = imageInfo;
  }
}

final launchingImageProvider =
    NotifierProvider<LaunchingImageNotifier, ImageInfo>(
  LaunchingImageNotifier.new,
);

final randomNameProvider = Provider.autoDispose(
  (ref) => generatePetname(ref.watch(vmNamesProvider)),
);

String imageName(ImageInfo imageInfo) {
  final result = '${imageInfo.os} ${imageInfo.release}';
  return imageInfo.aliases.any((a) => RegExp(r'\bcore\d{0,2}\b').hasMatch(a))
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
    final l10n = AppLocalizations.of(context)!;
    final imageInfo = ref.watch(launchingImageProvider);
    final randomName = ref.watch(randomNameProvider);
    final vmNames = ref.watch(vmNamesProvider);
    final deletedVms = ref.watch(deletedVmsProvider);
    final networksAsync = ref.watch(networksProvider);
    final networks = networksAsync.when(
      data: (data) => data,
      loading: () => const <String>{},
      error: (_, __) => const <String>{},
    );
    final bridgedNetworkSetting = ref.watch(bridgedNetworkProvider).when(
          data: (data) => data,
          loading: () => null,
          error: (_, __) => null,
        );

    final closeButton = IconButton(
      icon: const Icon(Icons.close),
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
    );

    final nameInput = SpecInput(
      label: l10n.launchFormNameLabel,
      autofocus: true,
      helper: l10n.launchFormNameHelper,
      hint: randomName,
      validator: nameValidator(vmNames, deletedVms, l10n),
      onSaved: (value) => launchRequest.instanceName =
          value.isNullOrBlank ? randomName : value!,
      width: 360,
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
    final isCore =
        imageInfo.aliases.any((a) => RegExp(r'\bcore\d{0,2}\b').hasMatch(a));
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
            ? l10n.bridgeNoNetworks
            : validBridgedNetwork
                ? l10n.launchFormBridgeConnect
                : l10n.launchFormBridgeNoValidNetwork;

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
      child: Text(l10n.mountsAddMount),
    );

    final saveMountButton = TextButton(
      onPressed: () {
        final mountFormState = mountFormKey.currentState;
        if (mountFormState == null) return;
        if (!mountFormState.validate()) return;
        mountFormState.save();
      },
      child: Text(l10n.dialogSave),
    );

    final cancelMountButton = OutlinedButton(
      onPressed: () => setState(() => addingMount = false),
      child: Text(l10n.dialogCancel),
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
            Text(l10n.launchFormTitle, style: const TextStyle(fontSize: 24)),
            const Spacer(),
            closeButton,
          ],
        ),
        const SizedBox(height: 20),
        Text(l10n.launchFormImageLabel, style: const TextStyle(fontSize: 18)),
        const SizedBox(height: 4),
        chosenImageName,
        const SizedBox(height: 16),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [nameInput, const Spacer()],
        ),
        const Divider(height: 60),
        SizedBox(
          height: 50,
          child:
              Text(l10n.resourcesTitle, style: const TextStyle(fontSize: 24)),
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
        SizedBox(
          height: 50,
          child: Text(l10n.bridgeTitle, style: const TextStyle(fontSize: 24)),
        ),
        bridgedSwitch,
        const Divider(height: 60),
        SizedBox(
          height: 50,
          child: Text(l10n.mountsTitle, style: const TextStyle(fontSize: 24)),
        ),
        mountPointsView,
        if (mountRequests.isNotEmpty) const SizedBox(height: 20),
        addingMount ? mountForm : addMountButton,
      ],
    );

    final launchButton = TextButton(
      onPressed: () => launch(imageInfo),
      child: Text(l10n.vmTableLaunch),
    );

    final launchAndConfigureNextButton = OutlinedButton(
      onPressed: () => launch(imageInfo, configureNext: true),
      child: Text(l10n.launchFormLaunchAndConfigureNext),
    );

    final cancelButton = OutlinedButton(
      onPressed: () => Scaffold.of(context).closeEndDrawer(),
      child: Text(l10n.dialogCancel),
    );

    return Stack(
      fit: StackFit.loose,
      children: [
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
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Divider(height: 30),
                Row(
                  children: [
                    launchButton,
                    const SizedBox(width: 16),
                    launchAndConfigureNextButton,
                    const SizedBox(width: 16),
                    cancelButton,
                  ],
                ),
              ],
            ),
          ),
        ),
      ],
    );
  }

  void launch(ImageInfo imageInfo, {bool configureNext = false}) {
    final formState = formKey.currentState;
    if (formState == null) return;
    final mountFormState = mountFormKey.currentState;

    if (!formState.validate()) return;
    if (!(mountFormState?.validate() ?? true)) return;

    mountFormState?.save();
    formState.save();

    launchRequest.image = imageInfo.aliases.first;
    if (imageInfo.hasRemoteName()) {
      launchRequest.remoteName = imageInfo.remoteName;
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
  AppLocalizations l10n,
) {
  return (String? value) {
    if (value!.isEmpty) {
      return null;
    }
    if (value.length < 2) {
      return l10n.usagePrimaryNameErrorTooShort;
    }
    if (RegExp(r'[^A-Za-z0-9\-]').hasMatch(value)) {
      return l10n.launchFormNameErrorInvalidChars;
    }
    if (RegExp(r'^[^A-Za-z]').hasMatch(value)) {
      return l10n.usagePrimaryNameErrorStartLetter;
    }
    if (RegExp(r'[^A-Za-z0-9]$').hasMatch(value)) {
      return l10n.usagePrimaryNameErrorEndChar;
    }
    if (existingNames.contains(value)) {
      return l10n.launchFormNameErrorInUse;
    }
    if (deletedNames.contains(value)) {
      return l10n.launchFormNameErrorDeletedInUse;
    }
    return null;
  };
}
