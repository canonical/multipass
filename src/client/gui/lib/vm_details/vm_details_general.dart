import 'dart:math';

import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';

import '../copyable_text.dart';
import '../extensions.dart';
import '../l10n/app_localizations.dart';
import '../providers.dart';
import 'cpu_sparkline.dart';
import 'memory_usage.dart';
import 'vm_action_buttons.dart';
import 'vm_details.dart';
import 'vm_status_icon.dart';

extension InstanceDetailsExtensions on InstanceDetails {
  String formattedCreationTime({required bool isLaunching}) {
    final ts = creationTimestamp;
    if (isLaunching && ts.seconds == 0 && ts.nanos == 0) return '-';
    return DateFormat('yyyy-MM-dd HH:mm:ss').format(ts.toDateTime());
  }
}

class VmDetailsHeader extends ConsumerWidget {
  final String name;

  const VmDetailsHeader(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final info = ref.watch(vmInfoProvider(name));

    final cpu = VmStat(
      width: 120,
      height: 35,
      label: l10n.vmStatCpuUsage,
      child: CpuSparkline(info.name),
    );

    final memory = VmStat(
      width: 110,
      height: 35,
      label: l10n.vmStatMemoryUsage,
      child: MemoryUsage(
        used: info.instanceInfo.memoryUsage,
        total: info.memoryTotal,
      ),
    );

    final disk = VmStat(
      width: 110,
      height: 35,
      label: l10n.vmStatDiskUsage,
      child: MemoryUsage(
        used: info.instanceInfo.diskUsage,
        total: info.diskTotal,
      ),
    );

    final currentLocation = ref.watch(vmScreenLocationProvider(name));
    final buttonStyle = Theme.of(context).outlinedButtonTheme.style;

    OutlinedButton locationButton(VmDetailsLocation location) {
      final style = buttonStyle?.copyWith(
        shape: const WidgetStatePropertyAll(RoundedRectangleBorder()),
        backgroundColor: location == currentLocation
            ? const WidgetStatePropertyAll(Color(0xff333333))
            : null,
        foregroundColor: location == currentLocation
            ? const WidgetStatePropertyAll(Colors.white)
            : null,
      );
      return OutlinedButton(
        style: style,
        child: Text(location.name.capitalize()),
        onPressed: () {
          ref.read(vmScreenLocationProvider(name).notifier).set(location);
        },
      );
    }

    final locationButtons = Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        locationButton(VmDetailsLocation.shells),
        locationButton(VmDetailsLocation.details),
      ],
    );

    final title = CopyableText(
      name.nonBreaking,
      style: const TextStyle(fontSize: 24, fontWeight: FontWeight.w300),
    );

    final usages = Row(
      mainAxisSize: MainAxisSize.min,
      children: [cpu, memory, disk].gap(width: _headerGap).toList(),
    );

    return Padding(
      padding: const EdgeInsets.all(20),
      child: _HeaderLayout(
        children: [title, locationButtons, usages, VmActionButtons(name)],
      ),
    );
  }
}

const double _headerGap = 40;
const double _headerRowSpacing = 20;
const double _nameMinWidth = 150;

/// Arranges the header's name, tabs, usages and actions over one to three rows.
class _HeaderLayout extends MultiChildRenderObjectWidget {
  const _HeaderLayout({required super.children});

  @override
  RenderObject createRenderObject(BuildContext context) =>
      _RenderHeaderLayout();
}

class _HeaderParentData extends ContainerBoxParentData<RenderBox> {}

class _RenderHeaderLayout extends RenderBox
    with
        ContainerRenderObjectMixin<RenderBox, _HeaderParentData>,
        RenderBoxContainerDefaultsMixin<RenderBox, _HeaderParentData> {
  @override
  void setupParentData(RenderBox child) {
    if (child.parentData is! _HeaderParentData) {
      child.parentData = _HeaderParentData();
    }
  }

  @override
  void performLayout() {
    assert(constraints.hasBoundedWidth);
    final width = constraints.maxWidth;
    final [name, tabs, usages, actions] = getChildrenAsList();

    for (final group in [tabs, usages, actions]) {
      group.layout(const BoxConstraints(), parentUsesSize: true);
    }
    name.layout(const BoxConstraints(), parentUsesSize: true);
    final t = tabs.size.width;
    final u = usages.size.width;
    final a = actions.size.width;
    // Never truncate the name below this, unless it is naturally shorter.
    final nameReserve = min(name.size.width, _nameMinWidth);

    final List<List<RenderBox>> rows;
    if (nameReserve + t + u + a + 3 * _headerGap <= width) {
      rows = [
        [name, tabs, usages, actions],
      ];
    } else if (nameReserve + t + u + 2 * _headerGap <= width) {
      rows = [
        [name, tabs, usages],
        [actions],
      ];
    } else if (u + a + _headerGap <= width) {
      rows = [
        [name, tabs],
        [usages, actions],
      ];
    } else {
      rows = [
        [name, tabs],
        [usages],
        [actions],
      ];
    }

    var top = 0.0;
    for (final row in rows) {
      if (row != rows.first) top += _headerRowSpacing;
      top += identical(row.first, name)
          ? _placeNameRow(row, name, width, top)
          : _placeRow(row, actions, width, top);
    }
    size = constraints.constrain(Size(width, top));
  }

  double _placeNameRow(
    List<RenderBox> row,
    RenderBox name,
    double width,
    double top,
  ) {
    final tail = row.sublist(1);
    final tailWidth = tail.fold<double>(0, (sum, b) => sum + b.size.width) +
        _headerGap * (tail.length - 1);
    final available = width - tailWidth - _headerGap;
    name.layout(
      BoxConstraints.tightFor(width: max(available, 0)),
      parentUsesSize: true,
    );

    final rowHeight = row.map((b) => b.size.height).reduce(max);
    _placeAt(name, 0, top, rowHeight);
    var x = width - tailWidth;
    for (final box in tail) {
      _placeAt(box, x, top, rowHeight);
      x += box.size.width + _headerGap;
    }
    return rowHeight;
  }

  double _placeRow(
    List<RenderBox> row,
    RenderBox actions,
    double width,
    double top,
  ) {
    final rowHeight = row.map((b) => b.size.height).reduce(max);
    if (row.length > 1) {
      _placeAt(row.first, 0, top, rowHeight);
      _placeAt(row.last, width - row.last.size.width, top, rowHeight);
    } else {
      final only = row.first;
      final x = identical(only, actions) ? width - only.size.width : 0.0;
      _placeAt(only, x, top, rowHeight);
    }
    return rowHeight;
  }

  void _placeAt(RenderBox child, double x, double top, double rowHeight) {
    final data = child.parentData as _HeaderParentData;
    data.offset = Offset(x, top + (rowHeight - child.size.height) / 2);
  }

  @override
  void paint(PaintingContext context, Offset offset) {
    defaultPaint(context, offset);
  }

  @override
  bool hitTestChildren(BoxHitTestResult result, {required Offset position}) {
    return defaultHitTestChildren(result, position: position);
  }
}

class VmStat extends StatelessWidget {
  final String label;
  final Widget child;
  final double width;
  final double height;

  const VmStat({
    super.key,
    required this.label,
    required this.child,
    required this.width,
    required this.height,
  });

  @override
  Widget build(BuildContext context) {
    final textScaler = MediaQuery.textScalerOf(context);
    return SizedBox(
      width: textScaler.scale(width),
      height: textScaler.scale(height),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label,
            style: const TextStyle(fontSize: 10, fontWeight: FontWeight.bold),
          ),
          Expanded(
            child: Align(alignment: Alignment.centerLeft, child: child),
          ),
        ],
      ),
    );
  }
}

class GeneralDetails extends ConsumerWidget {
  final String name;
  static const double baseVmStatHeight = 50;

  const GeneralDetails(this.name, {super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final info = ref.watch(vmInfoProvider(name));
    final isLaunching = ref.watch(isLaunchingProvider(name));

    final status = VmStat(
      width: 100,
      height: baseVmStatHeight,
      label: l10n.vmStatState,
      child: VmStatusIcon(info.instanceStatus.status, isLaunching: isLaunching),
    );

    final image = VmStat(
      width: 150,
      height: baseVmStatHeight,
      label: l10n.vmStatImage,
      child: CopyableText(info.instanceInfo.currentRelease),
    );

    final privateIp = VmStat(
      width: 150,
      height: baseVmStatHeight,
      label: l10n.vmStatPrivateIp,
      child: CopyableText(info.instanceInfo.ipv4.firstOrNull ?? '-'),
    );

    final publicIp = VmStat(
      width: 150,
      height: baseVmStatHeight,
      label: l10n.vmStatPublicIp,
      child: CopyableText(info.instanceInfo.ipv4.skip(1).firstOrNull ?? '-'),
    );

    final created = VmStat(
      width: 140,
      height: baseVmStatHeight,
      label: l10n.vmStatCreated,
      child: CopyableText(
        info.instanceInfo.formattedCreationTime(isLaunching: isLaunching),
      ),
    );

    final uptime = VmStat(
      width: 300,
      height: baseVmStatHeight,
      label: l10n.vmStatUptime,
      child: Text(info.instanceInfo.uptime),
    );

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SizedBox(
          height: baseVmStatHeight,
          child: Text(l10n.generalTitle, style: const TextStyle(fontSize: 24)),
        ),
        Wrap(
          spacing: 50,
          runSpacing: 25,
          children: [status, image, privateIp, publicIp, created, uptime],
        ),
      ],
    );
  }
}
