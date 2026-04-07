import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../extensions.dart';
import '../ffi.dart';
import '../l10n/app_localizations.dart';
import '../notifications.dart';
import '../providers.dart';
import '../tooltip.dart';
import 'cpus_slider.dart';
import 'disk_slider.dart';
import 'ram_slider.dart';
import 'vm_details.dart';

class ResourcesDetails extends ConsumerStatefulWidget {
  final String name;

  const ResourcesDetails(this.name, {super.key});

  @override
  ConsumerState<ResourcesDetails> createState() => _ResourcesDetailsState();
}

class _ResourcesDetailsState extends ConsumerState<ResourcesDetails> {
  final formKey = GlobalKey<FormState>();
  bool editing = false;

  late final cpusProvider = vmResourceProvider((
    name: widget.name,
    resource: VmResource.cpus,
  ));
  late final ramProvider = vmResourceProvider((
    name: widget.name,
    resource: VmResource.memory,
  ));
  late final diskProvider = vmResourceProvider((
    name: widget.name,
    resource: VmResource.disk,
  ));

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final cpus = ref.watch(cpusProvider).whenOrNull(data: int.tryParse);
    final ram = ref.watch(ramProvider).whenOrNull(data: memoryInBytes);
    final disk = ref.watch(diskProvider).whenOrNull(data: memoryInBytes);
    final stopped = ref.watch(
      vmInfoProvider(widget.name).select((info) {
        return info.instanceStatus.status == Status.STOPPED;
      }),
    );

    if (!stopped) editing = false;

    final cpusResource = !editing
        ? Text(
            l10n.resourcesCpusDisplay(cpus?.toString() ?? '…'),
            style: const TextStyle(fontSize: 16),
          )
        : CpusSlider(
            key: Key('cpus-$cpus'),
            initialValue: cpus,
            onSaved: (value) {
              if (value == null || value == cpus) return;
              ref.read(cpusProvider.notifier).set('$value').onError(
                    ref.notifyError((error) => l10n.resourcesFailedCpus('$error')),
                  );
            },
          );

    final ramResource = !editing
        ? Text(
            l10n.resourcesMemoryDisplay(ram.map(humanReadableMemory) ?? '…'),
            style: const TextStyle(fontSize: 16),
          )
        : RamSlider(
            key: Key('ram-$ram'),
            initialValue: ram,
            onSaved: (value) {
              if (value == null || value == ram) return;
              ref.read(ramProvider.notifier).set('${value}B').onError(
                    ref.notifyError((e) => l10n.resourcesFailedMemory('$e')),
                  );
            },
          );

    final diskResource = !editing
        ? Text(
            l10n.resourcesDiskDisplay(disk.map(humanReadableMemory) ?? '…'),
            style: const TextStyle(fontSize: 16),
          )
        : DiskSlider(
            key: Key('disk-$disk'),
            min: disk,
            initialValue: disk,
            onSaved: (value) {
              if (value == null || value == disk) return;
              ref.read(diskProvider.notifier).set('${value}B').onError(
                    ref.notifyError((e) => l10n.resourcesFailedDisk('$e')),
                  );
            },
          );

    final saveButton = TextButton(
      onPressed: () {
        if (!formKey.currentState!.validate()) return;
        formKey.currentState!.save();
        setState(() => editing = false);
        ref.read(activeEditPageProvider(widget.name).notifier).set(null);
      },
      child: Text(l10n.resourcesSaveChanges),
    );

    void configure() {
      setState(() => editing = true);
      ref
          .read(activeEditPageProvider(widget.name).notifier)
          .set(ActiveEditPage.resources);
    }

    final configureButton = Tooltip(
      visible: !stopped,
      message: l10n.vmDetailsStopToConfigure,
      child: OutlinedButton(
        onPressed: stopped ? configure : null,
        child: Text(l10n.dialogConfigure),
      ),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        formKey.currentState?.reset();
        setState(() => editing = false);
        ref.read(activeEditPageProvider(widget.name).notifier).set(null);
      },
      child: Text(l10n.dialogCancel),
    );

    return Form(
      key: formKey,
      autovalidateMode: AutovalidateMode.always,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              SizedBox(
                height: 50,
                child: Text(l10n.resourcesTitle, style: const TextStyle(fontSize: 24)),
              ),
              const Spacer(),
              editing ? cancelButton : configureButton,
            ],
          ),
          const SizedBox(height: 10),
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Expanded(child: cpusResource),
              const SizedBox(width: 86),
              Expanded(child: ramResource),
              const SizedBox(width: 86),
              Expanded(child: diskResource),
            ],
          ),
          if (editing)
            Padding(padding: const EdgeInsets.only(top: 16), child: saveButton),
        ],
      ),
    );
  }
}
