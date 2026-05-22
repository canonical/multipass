import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../l10n/app_localizations.dart';
import 'vm_table_headers.dart';

class EnabledHeadersNotifier extends Notifier<BuiltMap<String, bool>> {
  @override
  BuiltMap<String, bool> build() {
    return {for (final h in headers) h.name: true}.build();
  }

  void toggleHeader(String name, bool isSelected) {
    state = state.rebuild((set) => set[name] = isSelected);
  }
}

final enabledHeadersProvider =
    NotifierProvider<EnabledHeadersNotifier, BuiltMap<String, bool>>(
  EnabledHeadersNotifier.new,
);

class HeaderSelectionTile extends ConsumerWidget {
  final String name;
  final String label;

  const HeaderSelectionTile(this.name, this.label, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final enabledHeaders = ref.watch(enabledHeadersProvider);

    return CheckboxListTile(
      controlAffinity: ListTileControlAffinity.leading,
      title: Text(label, style: const TextStyle(color: Colors.black)),
      value: enabledHeaders[name],
      onChanged: (isSelected) => ref
          .read(enabledHeadersProvider.notifier)
          .toggleHeader(name, isSelected!),
    );
  }
}

class HeaderSelection extends StatelessWidget {
  const HeaderSelection({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final columnLabels = {
      'STATE': l10n.vmStatState,
      'CPU USAGE': l10n.vmStatCpuUsage,
      'MEMORY USAGE': l10n.vmStatMemoryUsage,
      'DISK USAGE': l10n.vmStatDiskUsage,
      'IMAGE': l10n.vmStatImage,
      'PRIVATE IP': l10n.vmStatPrivateIp,
      'PUBLIC IP': l10n.vmStatPublicIp,
    };
    return PopupMenuButton(
      position: PopupMenuPosition.under,
      itemBuilder: (_) => headers.skip(2).map((h) {
        return PopupMenuItem<void>(
          padding: EdgeInsets.zero,
          enabled: false,
          child: HeaderSelectionTile(h.name, columnLabels[h.name] ?? h.name),
        );
      }).toList(),
      child: Container(
        width: 120,
        height: 42,
        padding: const EdgeInsets.all(8),
        decoration: BoxDecoration(border: Border.all(color: Colors.grey)),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            SvgPicture.asset(
              'assets/settings.svg',
              colorFilter: const ColorFilter.mode(
                Color(0xff333333),
                BlendMode.srcIn,
              ),
            ),
            Text(
              l10n.vmTableColumnsButton,
              style: const TextStyle(fontWeight: FontWeight.bold),
            ),
          ],
        ),
      ),
    );
  }
}
