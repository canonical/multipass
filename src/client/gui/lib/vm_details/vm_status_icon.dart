import 'package:flutter/material.dart' hide Tooltip;

import '../colors.dart';
import '../extensions.dart';
import '../grpc_client.dart';
import '../l10n/app_localizations.dart';
import '../tooltip.dart';

Icon unknownStatusIcon(AppColors colors) {
  return Icon(Icons.help, color: colors.statusUnknown, size: 15);
}

Map<Status, Icon> statusIcons(AppColors colors) {
  final running = Icon(Icons.circle, color: colors.statusRunning, size: 10);
  final transitioning = Icon(
    Icons.more_horiz,
    color: colors.statusTransitioning,
    size: 15,
  );
  return {
    Status.RUNNING: running,
    Status.STOPPED: Icon(Icons.circle, color: colors.statusStopped, size: 10),
    Status.SUSPENDED: Icon(
      Icons.circle,
      color: colors.statusSuspended,
      size: 10,
    ),
    Status.RESTARTING: transitioning,
    Status.STARTING: transitioning,
    Status.SUSPENDING: transitioning,
    Status.DELETED: Icon(Icons.close, color: colors.statusDeleted, size: 15),
    Status.DELAYED_SHUTDOWN: running,
    Status.UNAVAILABLE: Icon(
      Icons.circle,
      color: colors.statusUnknown,
      size: 10,
    ),
  };
}

class VmStatusIcon extends StatelessWidget {
  final Status status;
  final bool isLaunching;

  const VmStatusIcon(this.status, {required this.isLaunching, super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final statusName = l10n.vmStatusLabel(
      !isLaunching ? status.name.toLowerCase() : 'launching',
    );

    final colors = context.colors;
    final icon = !isLaunching
        ? statusIcons(colors)[status] ?? unknownStatusIcon(colors)
        : SizedBox(
            width: 10,
            height: 10,
            child: FittedBox(
              fit: BoxFit.fill,
              child: CircularProgressIndicator(),
            ),
          );

    return Row(
      children: [
        icon,
        const SizedBox(width: 8),
        Expanded(
          child: Tooltip(
            message: statusName,
            child: Text(
              statusName.nonBreaking,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ),
      ],
    );
  }
}
