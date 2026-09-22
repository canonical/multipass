import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../confirmation_dialog.dart';
import '../extensions.dart';
import '../l10n/app_localizations.dart';
import '../notifications/notifications_provider.dart';
import '../platform/platform.dart';
import '../providers.dart';
import '../tooltip.dart';
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
    final l10n = AppLocalizations.of(context)!;
    final mounts = ref.watch(
      vmInfoProvider(widget.name).select((info) {
        return info.mountInfo.mountPaths.build();
      }),
    );
    final unavailable = ref.watch(
      vmInfoProvider(widget.name).select((info) {
        return info.instanceStatus.status == Status.UNAVAILABLE;
      }),
    );

    // When the zone is unavailable, the user can't configure it. Hide the mount details in that
    // case to prevent changes.
    final effectivePhase = unavailable ? MountDetailsPhase.idle : phase;

    if (unavailable && phase != MountDetailsPhase.idle) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) return;
        setState(() => phase = MountDetailsPhase.idle);
        ref.read(activeEditPageProvider(widget.name).notifier).set(null);
      });
    }

    final mountPointsView = MountPointsView(
      mounts: mounts,
      allowDelete: effectivePhase != MountDetailsPhase.idle,
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
      child: Text(l10n.commonSave),
    );

    final configureButton = OutlinedButton(
      onPressed: () {
        setState(() => phase = MountDetailsPhase.configure);
        ref
            .read(activeEditPageProvider(widget.name).notifier)
            .set(ActiveEditPage.mounts);
      },
      child: Text(l10n.commonConfigure),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        setState(() => phase = MountDetailsPhase.idle);
        ref.read(activeEditPageProvider(widget.name).notifier).set(null);
      },
      child: Text(l10n.commonCancel),
    );

    final addMountButton = OutlinedButton(
      onPressed: () {
        setState(() => phase = MountDetailsPhase.adding);
        ref
            .read(activeEditPageProvider(widget.name).notifier)
            .set(ActiveEditPage.mounts);
      },
      child: Text(l10n.mountsAddMount),
    );

    final detailsButton = Tooltip(
      visible: unavailable,
      message: l10n.vmDetailsUnavailableToMount,
      child: mounts.isEmpty ? addMountButton : configureButton,
    );
    final topRightButton =
        effectivePhase == MountDetailsPhase.idle ? detailsButton : cancelButton;

    return Form(
      key: formKey,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              SizedBox(
                height: 50,
                child: Text(l10n.mountsTitle,
                    style: const TextStyle(fontSize: 24)),
              ),
              const Spacer(),
              topRightButton,
            ],
          ),
          mountPointsView,
          const SizedBox(height: 20),
          if (effectivePhase == MountDetailsPhase.configure) addMountButton,
          if (effectivePhase == MountDetailsPhase.adding) ...[
            editableMountPoint,
            Padding(padding: const EdgeInsets.only(top: 16), child: saveButton),
          ],
        ],
      ),
    );
  }

  void doMount(MountRequest request) {
    final l10n = AppLocalizations.of(context)!;
    final grpcClient = ref.read(grpcClientProvider);
    final notificationsNotifier = ref.read(notificationsProvider.notifier);
    final target = request.targetPaths.first.targetPath;
    final description = '${request.sourcePath} into ${widget.name}:$target';

    request.targetPaths.first.instanceName = widget.name;
    notificationsNotifier.addOperation(
      grpcClient.mount(request),
      loading: l10n.mountNotificationLoading(description),
      onSuccess: (_) => l10n.mountNotificationSuccess(description),
      onError: (error) => l10n.mountNotificationError(description, '$error'),
    );
    setState(() => phase = MountDetailsPhase.idle);
    ref.read(activeEditPageProvider(widget.name).notifier).set(null);
  }

  void doUnmount(MountPaths mountPaths) {
    final l10n = AppLocalizations.of(context)!;
    final target = mountPaths.targetPath;
    final grpcClient = ref.read(grpcClientProvider);
    final notificationsNotifier = ref.read(notificationsProvider.notifier);

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => ConfirmationDialog(
        title: l10n.mountDeleteTitle,
        body: Text.rich(
          [
            l10n.mountDeleteBodyPrefix.span,
            '${mountPaths.sourcePath} ⭢ $target'.span.font('UbuntuMono'),
            l10n.mountDeleteBodySuffix(widget.name).span,
          ].spans,
        ),
        actionText: l10n.commonDelete,
        onAction: () {
          Navigator.pop(context);
          notificationsNotifier.addOperation(
            grpcClient.umount(widget.name, target),
            loading: l10n.unmountNotificationLoading(target, widget.name),
            onSuccess: (_) =>
                l10n.unmountNotificationSuccess(target, widget.name),
            onError: (error) {
              return l10n.unmountNotificationError(
                  target, widget.name, '$error');
            },
          );
        },
        inactionText: l10n.commonCancel,
        onInaction: () => Navigator.pop(context),
      ),
    );
  }
}
