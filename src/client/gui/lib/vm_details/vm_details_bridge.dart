import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';

import '../notifications.dart';
import '../l10n/app_localizations.dart';
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
    final l10n = AppLocalizations.of(context)!;
    final networks = ref.watch(networksProvider).when(
          data: (data) => data,
          loading: () => const <String>{},
          error: (_, __) => const <String>{},
        );
    final bridgedNetworkSetting = ref.watch(bridgedNetworkProvider).when(
          data: (data) => data,
          loading: () => null,
          error: (_, __) => null,
        );
    final bridged = ref.watch(
      bridgedProvider.select((value) {
        return value.when(
          data: (data) => data.toBoolOption.toNullable(),
          loading: () => null,
          error: (_, __) => null,
        );
      }),
    );
    final stopped = ref.watch(
      vmInfoProvider(widget.name).select((info) {
        return info.instanceStatus.status == Status.STOPPED;
      }),
    );

    if (!stopped) editing = false;

    final bridgedCheckbox = FormField<bool>(
      key: Key('bridged-$bridged'),
      initialValue: bridged ?? false,
      onSaved: (value) {
        if (value!) {
          ref.read(bridgedProvider.notifier).set(value.toString()).onError(
                ref.notifyError((e) => l10n.bridgeFailedNetwork('$e')),
              );
        }
      },
      builder: (field) {
        final validBridgedNetwork = networks.contains(bridgedNetworkSetting);
        final message = networks.isEmpty
              ? l10n.bridgeNoNetworks
              : validBridgedNetwork
                  ? l10n.bridgeEstablishedWarning
                  : l10n.bridgeNoValidNetwork;
        return CheckboxListTile(
          contentPadding: EdgeInsets.zero,
          controlAffinity: ListTileControlAffinity.leading,
          enabled: validBridgedNetwork,
          onChanged: field.didChange,
            title: Text(l10n.bridgeConnect),
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
      child: Text(l10n.dialogSave),
    );

    void configure() {
      setState(() => editing = true);
      ref
          .read(activeEditPageProvider(widget.name).notifier)
          .set(ActiveEditPage.bridge);
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
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              SizedBox(
                height: 50,
                child: Text(l10n.bridgeTitle, style: const TextStyle(fontSize: 24)),
              ),
              const Spacer(),
              if (editing)
                cancelButton
              else if (!(bridged ?? false))
                configureButton,
            ],
          ),
          editing
              ? SizedBox(width: 300, child: bridgedCheckbox)
              : Text(
                  bridged ?? false ? l10n.bridgeStatusConnected : l10n.bridgeStatusNotConnected,
                  style: const TextStyle(fontSize: 16),
                ),
          if (editing)
            Padding(padding: const EdgeInsets.only(top: 16), child: saveButton),
        ],
      ),
    );
  }
}
