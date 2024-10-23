import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../confirmation_dialog.dart';
import '../extensions.dart';
import '../notifications/notifications_provider.dart';
import '../platform/platform.dart';
import '../providers.dart';
import 'mount_points.dart';
import 'vm_details.dart';

class MountDetails extends ConsumerStatefulWidget {
  final String name;

  const MountDetails(this.name, {super.key});

  @override
  ConsumerState<MountDetails> createState() => _MountDetailsState();
}

enum MountDetailsPhase { idle, configure, adding }

class _MountDetailsState extends ConsumerState<MountDetails> {
  final formKey = GlobalKey<FormState>();
  var phase = MountDetailsPhase.idle;

  @override
  Widget build(BuildContext context) {
    final mounts = ref.watch(vmInfoProvider(widget.name).select((info) {
      return info.mountInfo.mountPaths.build();
    }));

    final mountPointsView = MountPointsView(
      mounts: mounts,
      allowDelete: phase != MountDetailsPhase.idle,
      onDelete: doUnmount,
    );

    final editableMountPoint = EditableMountPoint(
      existingTargets: mounts.map((m) => m.targetPath).toList(),
      initialSource: mounts.any((m) => m.sourcePath == mpPlatform.homeDirectory)
          ? null
          : mpPlatform.homeDirectory,
      onSaved: doMount,
    );

    final saveButton = TextButton(
      onPressed: () {
        if (!(formKey.currentState?.validate() ?? false)) return;
        formKey.currentState?.save();
      },
      child: const Text('Save'),
    );

    final configureButton = OutlinedButton(
      onPressed: () {
        setState(() => phase = MountDetailsPhase.configure);
        ref.read(activeEditPageProvider(widget.name).notifier).state =
            ActiveEditPage.mounts;
      },
      child: const Text('Configure'),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        setState(() => phase = MountDetailsPhase.idle);
        ref.read(activeEditPageProvider(widget.name).notifier).state = null;
      },
      child: const Text('Cancel'),
    );

    final addMountButton = OutlinedButton(
      onPressed: () {
        setState(() => phase = MountDetailsPhase.adding);
        ref.read(activeEditPageProvider(widget.name).notifier).state =
            ActiveEditPage.mounts;
      },
      child: const Text('Add mount'),
    );

    final topRightButton = phase == MountDetailsPhase.idle
        ? (mounts.isEmpty ? addMountButton : configureButton)
        : cancelButton;

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
            topRightButton,
          ]),
          mountPointsView,
          const SizedBox(height: 20),
          if (phase == MountDetailsPhase.configure) addMountButton,
          if (phase == MountDetailsPhase.adding) ...[
            editableMountPoint,
            Padding(
              padding: const EdgeInsets.only(top: 16),
              child: saveButton,
            ),
          ],
        ],
      ),
    );
  }

  void doMount(MountRequest request) {
    final grpcClient = ref.read(grpcClientProvider);
    final notificationsNotifier = ref.read(notificationsProvider.notifier);
    final target = request.targetPaths.first.targetPath;
    final description = '${request.sourcePath} into ${widget.name}:$target';

    request.targetPaths.first.instanceName = widget.name;
    notificationsNotifier.addOperation(
      grpcClient.mount(request),
      loading: 'Mounting $description',
      onSuccess: (_) => 'Mounted $description',
      onError: (error) => 'Failed to mount $description: $error',
    );
    setState(() => phase = MountDetailsPhase.idle);
    ref.read(activeEditPageProvider(widget.name).notifier).state = null;
  }

  void doUnmount(MountPaths mountPaths) {
    final target = mountPaths.targetPath;
    final grpcClient = ref.read(grpcClientProvider);
    final notificationsNotifier = ref.read(notificationsProvider.notifier);

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => ConfirmationDialog(
        title: 'Delete mount',
        body: Text.rich([
          'Are you sure you want to remove the mount\n'.span,
          '${mountPaths.sourcePath} ⭢ $target'.span.font('UbuntuMono'),
          ' from ${widget.name}?'.span,
        ].spans),
        actionText: 'Delete',
        onAction: () {
          Navigator.pop(context);
          notificationsNotifier.addOperation(
            grpcClient.umount(widget.name, target),
            loading: "Unmounting '$target' from ${widget.name}",
            onSuccess: (_) => "Unmounted '$target' from ${widget.name}",
            onError: (error) {
              return "Failed to unmount '$target' from ${widget.name}: $error";
            },
          );
        },
        inactionText: 'Cancel',
        onInaction: () => Navigator.pop(context),
      ),
    );
  }
}
