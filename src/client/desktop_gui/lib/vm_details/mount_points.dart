import 'package:basics/basics.dart';
import 'package:flutter/material.dart';

import '../ffi.dart';
import '../providers.dart';

class MountPoint extends StatelessWidget {
  final double width = 265;
  final VoidCallback onDelete;
  final FormFieldSetter<String> onSourceSaved;
  final FormFieldSetter<String> onTargetSaved;
  final String initialSource;
  final String initialTarget;

  const MountPoint({
    super.key,
    required this.onDelete,
    required this.onSourceSaved,
    required this.onTargetSaved,
    required this.initialSource,
    required this.initialTarget,
  });

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      SizedBox(
        width: width,
        child: TextFormField(
          onSaved: onSourceSaved,
          initialValue: initialSource,
        ),
      ),
      const SizedBox(width: 24),
      SizedBox(
        width: width,
        child: TextFormField(
          onSaved: onTargetSaved,
          initialValue: initialTarget,
        ),
      ),
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
  final mounts = <UniqueKey, (String, String)>{};

  void setExistingMounts() {
    for (final mount in widget.mountRequests) {
      mounts[UniqueKey()] = (
        mount.sourcePath,
        mount.targetPaths.first.targetPath,
      );
    }
  }

  @override
  void initState() {
    super.initState();
    setExistingMounts();
  }

  @override
  void didUpdateWidget(MountPointList oldWidget) {
    super.didUpdateWidget(oldWidget);
    setExistingMounts();
  }

  @override
  Widget build(BuildContext context) {
    final mountPoints = mounts.entries.map((entry) {
      final MapEntry(:key, value: (initialSource, initialTarget)) = entry;
      return Container(
        key: key,
        margin: const EdgeInsets.symmetric(vertical: 12),
        child: MountPoint(
          initialSource: initialSource,
          initialTarget: initialTarget,
          onDelete: () => setState(() => mounts.remove(key)),
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
      onPressed: () => setState(() => mounts[UniqueKey()] = ('', '')),
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
