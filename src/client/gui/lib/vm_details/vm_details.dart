import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';
import 'terminal_tabs.dart';
import 'vm_details_bridge.dart';
import 'vm_details_general.dart';
import 'vm_details_mounts.dart';
import 'vm_details_resources.dart';

enum VmDetailsLocation { shells, details }

final vmScreenLocationProvider = NotifierProvider.autoDispose
    .family<VmScreenLocationNotifier, VmDetailsLocation, String>(
  VmScreenLocationNotifier.new,
);

class VmScreenLocationNotifier extends Notifier<VmDetailsLocation> {
  VmScreenLocationNotifier(this.arg);
  final String arg;

  @override
  VmDetailsLocation build() {
    return VmDetailsLocation.shells;
  }

  void set(VmDetailsLocation location) {
    state = location;
  }
}

enum ActiveEditPage { resources, bridge, mounts }

final activeEditPageProvider = NotifierProvider.autoDispose
    .family<ActiveEditPageNotifier, ActiveEditPage?, String>(
  ActiveEditPageNotifier.new,
);

class ActiveEditPageNotifier extends Notifier<ActiveEditPage?> {
  ActiveEditPageNotifier(this.name);
  final String name;

  @override
  ActiveEditPage? build() {
    ref.listen(
      vmInfoProvider(name).select((info) => info.instanceStatus.status),
      (_, status) {
        final isBridgeOrResources = [
          ActiveEditPage.bridge,
          ActiveEditPage.resources,
        ].contains(state);

        if (isBridgeOrResources && status != Status.STOPPED) {
          ref.invalidateSelf();
        }
      },
    );
    return null;
  }

  void set(ActiveEditPage? page) {
    state = page;
  }
}

class VmDetailsScreen extends ConsumerWidget {
  final String name;

  const VmDetailsScreen(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final location = ref.watch(vmScreenLocationProvider(name));

    return Scaffold(
      body: Column(
        children: [
          VmDetailsHeader(name),
          Expanded(
            child: Stack(
              fit: StackFit.expand,
              children: [
                Visibility(
                  visible: location == VmDetailsLocation.shells,
                  maintainState: true,
                  child: TerminalTabs(name),
                ),
                Visibility(
                  visible: location == VmDetailsLocation.details,
                  child: VmDetails(name),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class VmDetails extends ConsumerWidget {
  final String name;

  const VmDetails(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final activeEditPage = ref.watch(activeEditPageProvider(name));

    return SingleChildScrollView(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            DisableSection(
              active: activeEditPage,
              letEnabledFor: const [],
              child: GeneralDetails(name),
            ),
            const Divider(height: 60),
            DisableSection(
              active: activeEditPage,
              letEnabledFor: const [ActiveEditPage.resources],
              child: ResourcesDetails(name),
            ),
            const Divider(height: 60),
            DisableSection(
              active: activeEditPage,
              letEnabledFor: const [ActiveEditPage.bridge],
              child: BridgedDetails(name),
            ),
            const Divider(height: 60),
            DisableSection(
              active: activeEditPage,
              letEnabledFor: const [ActiveEditPage.mounts],
              child: MountDetails(name),
            ),
          ],
        ),
      ),
    );
  }
}

class DisableSection extends StatelessWidget {
  final ActiveEditPage? active;
  final List<ActiveEditPage> letEnabledFor;
  final Widget child;

  const DisableSection({
    super.key,
    required this.active,
    required this.letEnabledFor,
    required this.child,
  });

  @override
  Widget build(BuildContext context) {
    final disabled = active != null && !letEnabledFor.contains(active);
    return IgnorePointer(
      ignoring: disabled,
      child: Opacity(opacity: disabled ? 0.5 : 1.0, child: child),
    );
  }
}
