import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ffi.dart';
import '../notifications.dart';
import '../providers.dart';
import '../tooltip.dart';
import 'spec_input.dart';
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
        ref.read(activeEditPageProvider(widget.name).notifier).state = null;
      },
      child: const Text('Save changes'),
    );

    void configure() {
      setState(() => editing = true);
      ref.read(activeEditPageProvider(widget.name).notifier).state =
          ActiveEditPage.resources;
    }

    final configureButton = Tooltip(
      visible: !stopped,
      message: 'Stop instance to configure',
      child: OutlinedButton(
        onPressed: stopped ? configure : null,
        child: const Text('Configure'),
      ),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        formKey.currentState?.reset();
        setState(() => editing = false);
        ref.read(activeEditPageProvider(widget.name).notifier).state = null;
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
