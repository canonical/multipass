import 'package:flutter/material.dart' hide Tooltip;

import '../extensions.dart';
import '../tooltip.dart';

class IpAddresses extends StatelessWidget {
  final Iterable<String> ips;

  const IpAddresses(this.ips, {super.key});

  @override
  Widget build(BuildContext context) {
    final firstIp = ips.firstOrNull ?? '-';
    final restIps = ips.skip(1).toList();

    return Row(children: [
      Expanded(
        child: Tooltip(
          message: firstIp,
          child: Text(firstIp.nonBreaking, overflow: TextOverflow.ellipsis),
        ),
      ),
      if (restIps.isNotEmpty)
        Badge.count(
          count: restIps.length,
          alignment: const AlignmentDirectional(-1.5, 0.4),
          textStyle: const TextStyle(fontSize: 10),
          largeSize: 14,
          child: PopupMenuButton<void>(
            icon: const Icon(Icons.keyboard_arrow_down),
            position: PopupMenuPosition.under,
            tooltip: 'Other IP addresses',
            splashRadius: 10,
            itemBuilder: (_) => [
              const PopupMenuItem(
                enabled: false,
                child: Text('Other IP addresses'),
              ),
              ...restIps.map((ip) => PopupMenuItem(child: Text(ip))),
            ],
          ),
        ),
    ]);
  }
}
