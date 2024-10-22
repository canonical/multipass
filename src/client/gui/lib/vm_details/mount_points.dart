import 'dart:io';

import 'package:basics/basics.dart';
import 'package:extended_text/extended_text.dart';
import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart' hide Tooltip;
import 'package:flutter_svg/flutter_svg.dart';

import '../ffi.dart';
import '../platform/platform.dart';
import '../providers.dart';
import '../tooltip.dart';
import 'spec_input.dart';

class EditableMountPoint extends StatefulWidget {
  final String? initialSource;
  final Iterable<String> existingTargets;
  final ValueChanged<MountRequest> onSaved;

  const EditableMountPoint({
    super.key,
    this.initialSource,
    required this.onSaved,
    required this.existingTargets,
  });

  @override
  State<EditableMountPoint> createState() => _EditableMountPointState();
}

class _EditableMountPointState extends State<EditableMountPoint> {
  var targetHint = '';
  final sourceController = TextEditingController();

  @override
  void initState() {
    super.initState();
    sourceController.addListener(() => setState(() {
          targetHint = defaultMountTarget(source: sourceController.text);
        }));
    sourceController.text = widget.initialSource ?? '';
  }

  @override
  void dispose() {
    sourceController.dispose();
    super.dispose();
  }

  void save(String source, String target) {
    target = target.isBlank ? targetHint : target;
    if (source.isNullOrBlank) return;
    final request = MountRequest(
      sourcePath: source,
      targetPaths: [TargetPathInfo(targetPath: target)],
      mountMaps: MountMaps(
        uidMappings: [IdMap(hostId: uid(), instanceId: defaultId())],
        gidMappings: [IdMap(hostId: gid(), instanceId: defaultId())],
      ),
    );
    widget.onSaved(request);
  }

  @override
  Widget build(BuildContext context) {
    final headers = DefaultTextStyle.merge(
      style: const TextStyle(color: Colors.black),
      child: const Row(children: [
        Expanded(
          child: Row(children: [
            Text('HOST DIRECTORY'),
            SizedBox(width: 8),
            Tooltip(
              message:
                  'A directory on your local machine that will be shared with the instance',
              child: Icon(Icons.info_outline, size: 20),
            ),
          ]),
        ),
        SizedBox(width: 24),
        Expanded(
          child: Row(children: [
            Text('GUEST DIRECTORY'),
            SizedBox(width: 8),
            Tooltip(
              message:
                  'A destination inside the instance for the shared directory.\n'
                  'If the destination directory already exists, its contents will not be visible until unmounting.',
              child: Icon(Icons.info_outline, size: 20),
            ),
          ]),
        ),
      ]),
    );

    final sourceField = ClippingTextField(
      controller: sourceController,
      validator: (value) {
        return value.isNullOrBlank ? 'Source cannot be empty' : null;
      },
    );

    final filePicker = OutlinedButton(
      onPressed: () async {
        final chosenSource = sourceController.text;
        final source = await getDirectoryPath(
          confirmButtonText: 'Select',
          initialDirectory: await Directory(chosenSource).exists()
              ? chosenSource
              : mpPlatform.homeDirectory,
        );
        if (source == null) return;
        sourceController.text = source;
      },
      child: const Text('Select'),
    );

    final targetField = SpecInput(
      hint: targetHint,
      onSaved: (target) => save(sourceController.text, target ?? ''),
      validator: (target) {
        target ??= '';
        target = target.isEmpty ? targetHint : target;
        return widget.existingTargets.contains(target)
            ? 'This path is used by another mount'
            : null;
      },
    );

    return Column(children: [
      headers,
      const SizedBox(height: 8),
      Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Expanded(
          child: Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Expanded(child: sourceField),
            const SizedBox(width: 10),
            SizedBox(height: 42, child: filePicker),
          ]),
        ),
        const SizedBox(width: 24),
        Expanded(child: targetField), // deleteButton,
      ]),
    ]);
  }
}

const middleOverflow = TextOverflowWidget(
  align: TextOverflowAlign.center,
  position: TextOverflowPosition.middle,
  child: Text('\u2026'),
);

class MountPointsView extends StatelessWidget {
  final Iterable<MountPaths> mounts;
  final bool allowDelete;
  final void Function(MountPaths mountPaths) onDelete;

  static const deleteButtonSize = 25.0;

  const MountPointsView({
    super.key,
    required this.mounts,
    required this.allowDelete,
    required this.onDelete,
  });

  @override
  Widget build(BuildContext context) {
    final mounts = this.mounts.toList();

    final headers = DefaultTextStyle.merge(
      style: const TextStyle(color: Colors.black),
      child: const Row(children: [
        Expanded(child: Text('HOST DIRECTORY')),
        SizedBox(width: 24),
        Expanded(child: Text('GUEST DIRECTORY')),
      ]),
    );

    const divider = Divider(
      height: 15,
      color: Colors.black38,
    );

    return Column(children: [
      if (mounts.isNotEmpty) ...[
        headers,
        divider,
      ],
      for (final mount in mounts) ...[
        buildEntry(mount),
        divider,
      ],
    ]);
  }

  Widget buildEntry(MountPaths mount) {
    final button = Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: IconButton(
        constraints: const BoxConstraints(),
        icon: SvgPicture.asset('assets/delete.svg', height: 20),
        iconSize: deleteButtonSize,
        padding: EdgeInsets.zero,
        splashRadius: 20,
        onPressed: () => onDelete(mount),
      ),
    );

    return DefaultTextStyle.merge(
      style: const TextStyle(fontSize: 16),
      child: SizedBox(
        height: 30,
        child: Row(children: [
          Expanded(
            child: ExtendedText(
              mount.sourcePath,
              maxLines: 1,
              overflowWidget: middleOverflow,
            ),
          ),
          const SizedBox(width: 24),
          Expanded(
            child: Row(children: [
              Expanded(
                child: ExtendedText(
                  mount.targetPath,
                  maxLines: 1,
                  overflowWidget: middleOverflow,
                ),
              ),
              if (allowDelete) button,
            ]),
          ),
        ]),
      ),
    );
  }
}

class ClippingTextField extends StatefulWidget {
  final TextEditingController controller;
  final FormFieldValidator<String>? validator;

  const ClippingTextField({
    super.key,
    required this.controller,
    this.validator,
  });

  @override
  State<ClippingTextField> createState() => _ClippingTextFieldState();
}

class _ClippingTextFieldState extends State<ClippingTextField> {
  var showClipped = true;
  final clippedFocusNode = FocusNode();
  final fieldFocusNode = FocusNode();

  @override
  void initState() {
    super.initState();
    fieldFocusNode.addListener(() {
      final hasFocus = fieldFocusNode.hasFocus;
      if (!hasFocus) {
        setState(() => showClipped = true);
      }
    });
    widget.controller.addListener(() {
      setState(() {
        // rebuild in order for the clipped widget to pick up the new value
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    if (showClipped) {
      final clippedText = ExtendedText(
        widget.controller.text,
        style: const TextStyle(fontSize: 16),
        maxLines: 1,
        overflowWidget: middleOverflow,
      );

      final clippedTextFormField = FormField<String>(
        validator: (_) => widget.validator?.call(widget.controller.text),
        builder: (field) => InputDecorator(
          decoration: InputDecoration(
            errorText: field.errorText,
          ),
          child: clippedText,
        ),
      );

      return GestureDetector(
        onTap: () => clippedFocusNode.requestFocus(),
        child: Focus(
          focusNode: clippedFocusNode,
          onFocusChange: (hasFocus) {
            if (hasFocus) {
              setState(() => showClipped = false);
              fieldFocusNode.requestFocus();
            }
          },
          child: clippedTextFormField,
        ),
      );
    }

    return SpecInput(
      controller: widget.controller,
      focusNode: fieldFocusNode,
      validator: widget.validator,
    );
  }
}
