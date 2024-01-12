import 'package:basics/basics.dart';
import 'package:flutter/material.dart';

import '../ffi.dart';
import '../providers.dart';

class MountPoint extends StatelessWidget {
  final double width = 265;
  final VoidCallback onDelete;
  final FormFieldSetter<String> onSourceSaved;
  final FormFieldSetter<String> onTargetSaved;

  const MountPoint({
    super.key,
    required this.onDelete,
    required this.onSourceSaved,
    required this.onTargetSaved,
  });

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      SizedBox(width: width, child: TextFormField(onSaved: onSourceSaved)),
      const SizedBox(width: 24),
      SizedBox(width: width, child: TextFormField(onSaved: onTargetSaved)),
      const SizedBox(width: 24),
      SizedBox(
        height: 42,
        child: OutlinedButton(
          onPressed: onDelete,
          child: const Icon(Icons.delete_outline, color: Colors.grey),
        ),
      ),
    ]);
  }
}

class MountPointList extends StatefulWidget {
  final List<MountRequest> mountRequests;

  const MountPointList({super.key, required this.mountRequests});

  @override
  State<MountPointList> createState() => _MountPointListState();
}

class _MountPointListState extends State<MountPointList> {
  final keys = <UniqueKey>{};

  @override
  Widget build(BuildContext context) {
    final mountPoints = keys.map((key) {
      return Container(
        key: key,
        margin: const EdgeInsets.symmetric(vertical: 12),
        child: MountPoint(
          onDelete: () => setState(() => keys.remove(key)),
          onSourceSaved: (value) {
            widget.mountRequests.add(MountRequest(
              sourcePath: value,
              mountMaps: MountMaps(
                uidMappings: [IdMap(hostId: uid(), instanceId: default_id())],
                gidMappings: [IdMap(hostId: gid(), instanceId: default_id())],
              ),
            ));
          },
          onTargetSaved: (value) {
            final request = widget.mountRequests.last;
            request.targetPaths.add(TargetPathInfo(
              targetPath: value.isNullOrBlank ? request.sourcePath : value!,
            ));
          },
        ),
      );
    }).toList();

    final addMountPoint = OutlinedButton.icon(
      onPressed: () => setState(() => keys.add(UniqueKey())),
      label: const Text('Add mount point'),
      icon: const Icon(Icons.add),
    );

    const labels = Row(children: [
      SizedBox(
        width: 265 + 24,
        child: Text('Source path', style: TextStyle(fontSize: 16)),
      ),
      Text('Target path', style: TextStyle(fontSize: 16)),
    ]);

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      if (mountPoints.isNotEmpty) labels,
      ...mountPoints,
      addMountPoint,
    ]);
  }
}
