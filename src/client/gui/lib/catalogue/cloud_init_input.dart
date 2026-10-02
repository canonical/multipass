import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:re_editor/re_editor.dart';
import 'package:re_highlight/languages/yaml.dart';
import 'package:re_highlight/styles/atom-one-light.dart';
import 'package:yaml/yaml.dart';
import 'package:file_selector/file_selector.dart';

import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/platform/platform.dart';
import 'package:multipass_gui/notifications.dart';

final initialUserData = '''
#cloud-config

## Update package lists before installing packages.
package_update: false

## Upgrade installed packages.
package_upgrade: false

## Install additional packages.
# packages:
#   - curl
#   - jq

## Create or replace files in the instance.
# write_files:
#   - path: /etc/motd
#     permissions: '0644'
#     content: |
#       Welcome to your Multipass instance!

## Run commands once during the instance's first boot.
# runcmd:
#   - echo "Hello from cloud-init"
''';

class CloudInitInput extends ConsumerStatefulWidget {
  final FormFieldSetter<String> onSaved;

  const CloudInitInput({super.key, required this.onSaved});

  @override
  ConsumerState<CloudInitInput> createState() => _CloudInitInputState();
}

String? validateCloudInitUserData(String? value) {
  // TODO: Validate using the actual cloud-init user-data schema
  const marker = "#cloud-config";
  try {
    if (value == null) return null;

    // Check that the first line is #cloud-config
    if (!value.startsWith(marker)) {
      return "Expected first line to be: #cloud-config";
    }
    if (value.length > marker.length &&
        value[marker.length] != '\n' &&
        value[marker.length] != '\r') {
      return "Expected first line to be: #cloud-config";
    }

    final document = loadYaml(value);
    if (document != null && document is! YamlMap) {
      return 'Expected a mapping at the document root.';
    }
    return null;
  } on YamlException catch (error) {
    return error.message;
  }
}

class _CloudInitInputState extends ConsumerState<CloudInitInput> {
  late final CodeLineEditingController _controller;

  @override
  void initState() {
    _controller = CodeLineEditingController.fromText(initialUserData);
    super.initState();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  Future<void> _pickFile() async {
    final l10n = AppLocalizations.of(context)!;

    try {
      final file = await openFile(
        confirmButtonText: l10n.launchFormCloudInitLoadFile,
        initialDirectory: mpPlatform.homeDirectory,
      );
      if (file == null) return;

      final content = await file.readAsString();
      if (!mounted) return;
      setState(() {
        _controller.text = content;
      });
    } catch (error) {
      if (!mounted) return;
      ref
          .read(notificationsProvider.notifier)
          .addError(l10n.launchFormCloudInitFileError(error.toString()));
    }
  }

  CodeEditor _createEditor(FormFieldState<String> field) => CodeEditor(
      controller: _controller,
      onChanged: (_) => field.didChange(_controller.text),
      wordWrap: false,
      autofocus: false,
      style: CodeEditorStyle(
          fontFamily: 'UbuntuMono',
          fontSize: 16,
          backgroundColor: const Color(0xfff2f2f2),
          codeTheme: CodeHighlightTheme(
              languages: {'yaml': CodeHighlightThemeMode(mode: langYaml)},
              theme: atomOneLightTheme)),
      indicatorBuilder:
          (context, editingController, chunkController, notifier) =>
              Row(children: [
                DefaultCodeLineNumber(
                    controller: editingController, notifier: notifier),
                const VerticalDivider(color: Color(0xffcccccc))
              ]));

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Text(l10n.launchFormCloudInitDescription),
      const SizedBox(height: 12),
      FormField<String>(
          initialValue: initialUserData,
          // The validator is called whenever the text is changed due to
          // autovalidateMode being set to always in `launch_form.dart`.
          // If this proves to be slow, we can use a debouncer to delay
          // validation
          validator: (value) {
            final error = validateCloudInitUserData(value);
            return error == null
                ? null
                : l10n.launchFormCloudInitInvalidYaml(error);
          },
          onSaved: widget.onSaved,
          builder: (field) => InputDecorator(
              decoration: InputDecoration(
                  labelText: l10n.launchFormCloudInitUserDataLabel,
                  errorText: field.errorText,
                  labelStyle:
                      TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              child: SizedBox(height: 220, child: _createEditor(field)))),
      const SizedBox(height: 12),
      Row(children: [
        OutlinedButton(
          onPressed: _pickFile,
          child: Text(l10n.launchFormCloudInitLoadFile),
        ),
        const SizedBox(width: 12),
        OutlinedButton(
          onPressed: () => setState(() {
            _controller.text = initialUserData;
          }),
          child: Text(l10n.launchFormCloudInitReset),
        ),
      ])
    ]);
  }
}
