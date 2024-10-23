import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';

import '../notifications.dart';
import '../providers.dart';
import '../tooltip.dart';
import 'vm_details.dart';

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
  late final bridgedProvider = vmResourceProvider((
    name: widget.name,
    resource: VmResource.bridged,
  ));

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
                ref.notifyError((e) => 'Failed to set bridged network: $e'),
              );
        }
      },
      builder: (field) {
        final validBridgedNetwork = networks.contains(bridgedNetworkSetting);
        final message = networks.isEmpty
            ? 'No networks found.'
            : validBridgedNetwork
                ? "Once connection is established, you won't be able to unset it."
                : 'No valid bridged network is set.';

        return CheckboxListTile(
          contentPadding: EdgeInsets.zero,
          controlAffinity: ListTileControlAffinity.leading,
          enabled: validBridgedNetwork,
          onChanged: field.didChange,
          title: const Text('Connect to bridged network.'),
          value: field.value!,
          visualDensity: VisualDensity.standard,
          subtitle: Text(message),
        );
      },
    );

    final saveButton = TextButton(
      onPressed: () {
        formKey.currentState?.save();
        setState(() => editing = false);
      },
      child: const Text('Save'),
    );

    void configure() {
      setState(() => editing = true);
      ref.read(activeEditPageProvider(widget.name).notifier).state =
          ActiveEditPage.bridge;
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
