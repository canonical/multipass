import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';

import '../ffi.dart';
import '../providers.dart';

class MountPoint extends StatefulWidget {
  final VoidCallback onDelete;
  final Function(String source, String target) onSaved;
  final String initialSource;
  final String initialTarget;

  const MountPoint({
    super.key,
    required this.onDelete,
    required this.onSaved,
    required this.initialSource,
    required this.initialTarget,
  });

  @override
  State<MountPoint> createState() => _MountPointState();
}

class _MountPointState extends State<MountPoint> {
  final width = 265.0;
  String? savedSource;
  String? savedTarget;

  void save() {
    if (savedSource != null && savedTarget != null) {
      widget.onSaved(savedSource!, savedTarget!);
      savedSource = null;
      savedTarget = null;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      SizedBox(
        width: width,
        child: TextFormField(
          initialValue: widget.initialSource,
          onSaved: (value) {
            savedSource = value;
            save();
          },
        ),
      ),
      const SizedBox(width: 24),
      SizedBox(
        width: width,
        child: TextFormField(
          initialValue: widget.initialTarget,
          onSaved: (value) {
            savedTarget = value;
            save();
          },
        ),
      ),
      const SizedBox(width: 24),
      SizedBox(
        height: 42,
        child: OutlinedButton(
          onPressed: widget.onDelete,
          child: const Icon(Icons.delete_outline, color: Colors.grey),
        ),
      ),
    ]);
  }
}

class MountPointList extends StatefulWidget {
  final BuiltList<MountRequest> initialMountRequests;
  final Function(BuiltList<MountRequest>) onSaved;

  MountPointList({
    super.key,
    required this.onSaved,
    BuiltList<MountRequest>? initialMountRequests,
  }) : initialMountRequests = initialMountRequests ?? BuiltList();

  @override
  State<MountPointList> createState() => _MountPointListState();
}

class _MountPointListState extends State<MountPointList> {
  final mounts = <UniqueKey, (String, String)>{};
  final mountRequests = <MountRequest>[];

  void save(MountRequest request) {
    mountRequests.add(request);
    if (mounts.length == mountRequests.length) {
      widget.onSaved(mountRequests.build());
      mountRequests.clear();
    }
  }

  void setExistingMounts() {
    for (final mount in widget.initialMountRequests) {
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
          onSaved: (source, target) {
            final request = MountRequest(
              sourcePath: source,
              targetPaths: [
                TargetPathInfo(targetPath: target.isBlank ? source : target)
              ],
              mountMaps: MountMaps(
                uidMappings: [IdMap(hostId: uid(), instanceId: default_id())],
                gidMappings: [IdMap(hostId: gid(), instanceId: default_id())],
              ),
            );
            save(request);
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
