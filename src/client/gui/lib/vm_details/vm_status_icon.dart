import 'package:flutter/material.dart';

import '../extensions.dart';
import '../grpc_client.dart';

const unknownIcon = Icon(Icons.help, color: Color(0xff757575), size: 15);
const icons = {
  Status.RUNNING: Icon(Icons.circle, color: Color(0xff0C8420), size: 10),
  Status.STOPPED: Icon(Icons.circle, color: Colors.black, size: 10),
  Status.SUSPENDED: Icon(Icons.circle, color: Color(0xff666666), size: 10),
  Status.RESTARTING: Icon(Icons.more_horiz, color: Color(0xff757575), size: 15),
  Status.STARTING: Icon(Icons.more_horiz, color: Color(0xff757575), size: 15),
  Status.SUSPENDING: Icon(Icons.more_horiz, color: Color(0xff757575), size: 15),
  Status.DELETED: Icon(Icons.close, color: Colors.redAccent, size: 15),
  Status.DELAYED_SHUTDOWN:
      Icon(Icons.circle, color: Color(0xff0C8420), size: 10),
};

class VmStatusIcon extends StatelessWidget {
  final Status status;

  const VmStatusIcon(this.status, {super.key});

  @override
  Widget build(BuildContext context) {
    final statusName = status.name.toLowerCase().replaceAll('_', ' ');
    return Row(children: [
      icons[status] ?? unknownIcon,
      const SizedBox(width: 8),
      Expanded(
        child: Tooltip(
          message: statusName,
          child: Text(statusName.nonBreaking, overflow: TextOverflow.ellipsis),
        ),
      ),
    ]);
  }
}
