import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';

import '../extensions.dart';
import '../ffi.dart';
import '../providers.dart';

class MountPoint extends StatefulWidget {
  final VoidCallback onDelete;
  final Function(MountRequest mountRequest) onSaved;
  final String initialSource;
  final String initialTarget;
  final Widget Function(
    Widget sourceField,
    Widget targetField,
    Widget deleteButton,
  ) builder;

  const MountPoint({
    super.key,
    required this.onDelete,
    required this.onSaved,
    required this.initialSource,
    required this.initialTarget,
    required this.builder,
  });

  @override
  State<MountPoint> createState() => _MountPointState();
}

class _MountPointState extends State<MountPoint> {
  String? source;
  String? target;

  void save() {
    final (savedSource, savedTarget) = (source, target);
    if (savedSource.isNullOrBlank || savedTarget == null) return;

    final targetPath = TargetPathInfo(
      targetPath: savedTarget.isBlank ? savedSource : savedTarget,
    );

    final request = MountRequest(
      sourcePath: savedSource,
      targetPaths: [targetPath],
      mountMaps: MountMaps(
        uidMappings: [IdMap(hostId: uid(), instanceId: default_id())],
        gidMappings: [IdMap(hostId: gid(), instanceId: default_id())],
      ),
    );

    widget.onSaved(request);
    source = null;
    target = null;
  }

  @override
  Widget build(BuildContext context) {
    final sourceField = TextFormField(
      initialValue: widget.initialSource,
      onSaved: (value) => source = value ?? '',
      validator: (value) {
        return value.isNullOrBlank ? 'Source cannot be empty' : null;
      },
    );

    final targetField = TextFormField(
      initialValue: widget.initialTarget,
      onSaved: (value) {
        target = value ?? '';
        save();
      },
    );

    final deleteButton = SizedBox(
      height: 42,
      child: OutlinedButton(
        onPressed: widget.onDelete,
        child: const Icon(Icons.delete_outline, color: Colors.grey),
      ),
    );

    return widget.builder(sourceField, targetField, deleteButton);
  }
}

class MountPointList extends StatefulWidget {
  final double? width;
  final ValueChanged<BuiltList<MountRequest>> onSaved;
  final bool showLabels;

  const MountPointList({
    super.key,
    this.width,
    required this.onSaved,
    this.showLabels = true,
  });

  @override
  State<MountPointList> createState() => _MountPointListState();
}

class _MountPointListState extends State<MountPointList> {
  static const gap = 24.0;
  final mounts = <UniqueKey, (String, String)>{};
  final savedMountRequests = <MountRequest>[];

  void addEntry() => setState(() => mounts[UniqueKey()] = ('', ''));

  void onEntryDeleted(Key key) => setState(() => mounts.remove(key));

  void onEntrySaved(MountRequest request) {
    savedMountRequests.add(request);
    if (mounts.length == savedMountRequests.length) {
      widget.onSaved(savedMountRequests.build());
      savedMountRequests.clear();
    }
  }

  Widget widthWrapper(Widget child) {
    final width = widget.width;
    return width == null
        ? Expanded(child: child)
        : SizedBox(width: width, child: child);
  }

  @override
  Widget build(BuildContext context) {
    final mountPoints = mounts.entries.map((entry) {
      final MapEntry(:key, value: (initialSource, initialTarget)) = entry;
      return Container(
        key: key,
        margin: const EdgeInsets.symmetric(vertical: gap / 2),
        child: MountPoint(
          initialSource: initialSource,
          initialTarget: initialTarget,
          onSaved: onEntrySaved,
          onDelete: () => onEntryDeleted(key),
          builder: (sourceField, targetField, deleteButton) => Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              widthWrapper(sourceField),
              widthWrapper(targetField),
              deleteButton,
            ].gap(width: gap).toList(),
          ),
        ),
      );
    }).toList();

    final addMountPoint = OutlinedButton.icon(
      onPressed: addEntry,
      label: const Text('Add mount point'),
      icon: const Icon(Icons.add),
    );

    final labels = DefaultTextStyle.merge(
      style: const TextStyle(fontSize: 16),
      child: Row(
        children: [
          widthWrapper(const Text('Source path')),
          widthWrapper(const Text('Target path')),
        ].gap(width: gap).toList(),
      ),
    );

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      if (widget.showLabels && mountPoints.isNotEmpty) labels,
      ...mountPoints,
      addMountPoint,
    ]);
  }
}

class MountPointsView extends StatefulWidget {
  final Iterable<MountPaths> existingMounts;
  final bool editing;
  final ValueChanged<BuiltList<String>> onSaved;

  const MountPointsView({
    super.key,
    required this.existingMounts,
    required this.editing,
    required this.onSaved,
  });

  @override
  State<MountPointsView> createState() => _MountPointsViewState();
}

class _MountPointsViewState extends State<MountPointsView> {
  static const deleteButtonSize = 25.0;

  final toUnmount = <String>{};

  @override
  void didUpdateWidget(MountPointsView oldWidget) {
    super.didUpdateWidget(oldWidget);
    toUnmount.clear();
  }

  Widget buildEntry(MountPaths mount) {
    final contains = toUnmount.contains(mount.targetPath);

    final button = Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: IconButton(
        constraints: const BoxConstraints(),
        icon: Icon(contains ? Icons.undo : Icons.delete),
        iconSize: deleteButtonSize,
        padding: EdgeInsets.zero,
        splashRadius: 20,
        onPressed: () => setState(() {
          if (!toUnmount.remove(mount.targetPath)) {
            toUnmount.add(mount.targetPath);
          }
        }),
      ),
    );

    return DefaultTextStyle.merge(
      style: TextStyle(
        decoration: contains ? TextDecoration.lineThrough : null,
        fontSize: 16,
      ),
      child: SizedBox(
        height: 30,
        child: Row(children: [
          Expanded(child: Text(mount.sourcePath)),
          Expanded(child: Text(mount.targetPath)),
          widget.editing
              ? button
              : const SizedBox(width: deleteButtonSize + 32),
        ]),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return FormField<BuiltList<String>>(
      onSaved: (_) => widget.onSaved(toUnmount.toBuiltList()),
      builder: (_) {
        return Column(children: [
          if (widget.existingMounts.isNotEmpty)
            DefaultTextStyle.merge(
              style: const TextStyle(fontWeight: FontWeight.bold),
              child: const Row(children: [
                Expanded(child: Text('SOURCE PATH')),
                Expanded(child: Text('TARGET PATH')),
                SizedBox(width: deleteButtonSize + 32),
              ]),
            ),
          for (final mount in widget.existingMounts) ...[
            const Divider(height: 10),
            buildEntry(mount),
          ],
        ]);
      },
    );
  }
}
