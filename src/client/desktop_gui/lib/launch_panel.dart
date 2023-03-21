import 'package:basics/basics.dart' as basics hide ListBasics;
import 'package:collection/collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:rxdart/rxdart.dart';

import 'generated/multipass.pb.dart';
import 'grpc_client.dart';
import 'name_generator.dart';
import 'snackbar.dart';

final launchOperationProvider = StateProvider<Stream<LaunchReply>?>(
  (_) => null,
);
final cancelLaunchProvider = StateProvider<Future<void> Function()?>(
  (_) => null,
);
final randomNameProvider = Provider.autoDispose(
  (ref) => randomInstanceName(ref.watch(vmNamesProvider)),
);
final imagesProvider = Provider.autoDispose(
  (ref) {
    ref.watch(grpcClientProvider).find().then((value) => ref.state = value);
    return <FindReply_ImageInfo>[];
  },
);

class LaunchPanel extends ConsumerWidget {
  LaunchPanel({super.key});

  final formKey = GlobalKey<FormState>();
  final LaunchRequest request = LaunchRequest();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final hasImages = ref.watch(
      imagesProvider.select((images) => images.isNotEmpty),
    );
    final header = Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        const Text('Launch instance', style: TextStyle(fontSize: 22)),
        IconButton(
          onPressed: () => Scaffold.of(context).closeEndDrawer(),
          icon: const Icon(Icons.close),
        )
      ],
    );

    final launchOperation = ref.watch(launchOperationProvider);
    final body = launchOperation == null
        ? _buildLaunchForm(context, ref)
        : _buildLaunchProgress(context, ref, launchOperation);

    final launchButton = ElevatedButton(
      onPressed: launchOperation == null && hasImages
          ? () {
              if (!formKey.currentState!.validate()) return;

              request.clear();
              formKey.currentState!.save();
              final launchOperation =
                  ref.read(grpcClientProvider).launch(request);
              ref.read(cancelLaunchProvider.notifier).state =
                  launchOperation.cancel;
              ref.read(launchOperationProvider.notifier).state =
                  BehaviorSubject()
                    ..addStream(
                      launchOperation,
                      cancelOnError: true,
                    );
            }
          : null,
      style: ButtonStyle(
        foregroundColor: MaterialStateProperty.resolveWith(
          (states) => states.contains(MaterialState.disabled)
              ? Colors.black54
              : Colors.white,
        ),
        backgroundColor: MaterialStateProperty.resolveWith(
          (states) => states.contains(MaterialState.disabled)
              ? Colors.grey
              : const Color(0xff006ada),
        ),
      ),
      child: const Text('Launch'),
    );

    final cancelButton = OutlinedButton(
      onPressed: () {
        Scaffold.of(context).closeEndDrawer();
        ref
            .read(cancelLaunchProvider)
            ?.call()
            .then((_) => ref.invalidate(launchOperationProvider));
      },
      style: const ButtonStyle(
        foregroundColor: MaterialStatePropertyAll(Colors.black),
      ),
      child: const Text('Cancel'),
    );

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          header,
          body,
          const Spacer(),
          const Divider(),
          Row(children: [launchButton, const SizedBox(width: 8), cancelButton])
        ],
      ),
    );
  }

  Widget _buildLaunchProgress(
    BuildContext context,
    WidgetRef ref,
    Stream<LaunchReply> stream,
  ) {
    return StreamBuilder(
        stream: stream,
        builder: (_, snapshot) {
          if (snapshot.hasError) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              Scaffold.of(context).closeEndDrawer();
              ref.invalidate(launchOperationProvider);
              showSnackBarMessage(
                context,
                'Failed to launch ${request.instanceName}: ${snapshot.error}.',
                failure: true,
              );
            });
            return Container();
          }

          if (!snapshot.hasData) return Container();

          final reply = snapshot.data!;
          int? downloaded;
          String message = reply.replyMessage;
          switch (reply.whichCreateOneof()) {
            case LaunchReply_CreateOneof.launchProgress:
              downloaded = int.tryParse(reply.launchProgress.percentComplete);
              message = 'Downloading image';
              if (reply.launchProgress.type ==
                  LaunchProgress_ProgressTypes.VERIFY) {
                downloaded = null;
                message = 'Verifying image';
              }
              break;
            case LaunchReply_CreateOneof.createMessage:
              message = reply.createMessage;
              break;
            case LaunchReply_CreateOneof.vmInstanceName:
              WidgetsBinding.instance.addPostFrameCallback((_) {
                Scaffold.of(context).closeEndDrawer();
                ref.invalidate(launchOperationProvider);
                showSnackBarMessage(
                  context,
                  'Launched ${request.instanceName} successfully.',
                );
              });
              return Container();
            default:
          }

          return Row(children: [
            Padding(
              padding: const EdgeInsets.all(8.0),
              child: Stack(
                alignment: Alignment.center,
                children: [
                  Text(downloaded == null ? '' : '$downloaded%'),
                  CircularProgressIndicator(
                      value: downloaded == null
                          ? null
                          : downloaded.toDouble() / 100),
                ],
              ),
            ),
            Text(message),
          ]);
        });
  }

  Widget _buildLaunchForm(BuildContext context, WidgetRef ref) {
    final vmNames = ref.watch(vmNamesProvider);
    final images = ref.watch(imagesProvider);
    final randomName = ref.watch(randomNameProvider);
    final defaultImage = images.firstWhereOrNull((image) => image.aliasesInfo
        .map((aliasInfo) => aliasInfo.alias)
        .contains('default'));

    final nameField = TextFormField(
      autofocus: true,
      onSaved: (value) =>
          request.instanceName = value.isNullOrBlank ? randomName : value!,
      validator: _nameValidator(vmNames),
      decoration: InputDecoration(
        floatingLabelBehavior: FloatingLabelBehavior.always,
        labelText: 'Instance name',
        helperText:
            'Names are immutable. If no name is provided, Multipass will randomly generate a name.',
        helperMaxLines: 3,
        errorMaxLines: 2,
        hintText: randomName,
        border: const OutlineInputBorder(),
      ),
    );

    final imageField = DropdownButtonFormField<FindReply_ImageInfo>(
      value: defaultImage,
      selectedItemBuilder: (_) =>
          images.map((e) => Text(imageName(e.aliasesInfo.first))).toList(),
      items: images.map(_buildDropdownItem).toList(),
      onChanged: (_) {},
      onSaved: (value) {
        final aliasInfo = value!.aliasesInfo.first;
        request.image = aliasInfo.alias;
        if (aliasInfo.hasRemoteName()) {
          request.remoteName = aliasInfo.remoteName;
        }
      },
      decoration: const InputDecoration(
        labelText: 'Image',
        border: OutlineInputBorder(),
      ),
    );

    final cpusField = TextFormField(
      initialValue: '1',
      validator: (value) => (int.tryParse(value!) ?? 0) > 0
          ? null
          : 'Number of CPUs must be greater than 0',
      onSaved: (value) => request.numCores = int.parse(value!),
      inputFormatters: [FilteringTextInputFormatter.digitsOnly],
      decoration: const InputDecoration(
        labelText: 'CPUs',
        helperText: 'Number of cores',
        border: OutlineInputBorder(),
      ),
    );

    final diskField = TextFormField(
      initialValue: '5',
      validator: _memorySizeValidator,
      onSaved: (value) => request.diskSpace =
          double.tryParse(value!) != null ? '${value}GB' : value,
      decoration: const InputDecoration(
        labelText: 'Disk',
        helperText: 'Default unit in GB',
        border: OutlineInputBorder(),
      ),
    );

    final memoryField = TextFormField(
      initialValue: '1',
      validator: _memorySizeValidator,
      onSaved: (value) => request.memSize =
          double.tryParse(value!) != null ? '${value}GB' : value,
      decoration: const InputDecoration(
        labelText: 'Memory',
        helperText: 'Default unit in GB',
        border: OutlineInputBorder(),
      ),
    );

    return Form(
      autovalidateMode: AutovalidateMode.always,
      key: formKey,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          nameField,
          imageField,
          Row(children: [
            Expanded(child: cpusField),
            const SizedBox(width: 16),
            Expanded(child: memoryField),
          ]),
          Row(children: [
            Expanded(child: diskField),
            const SizedBox(width: 16),
            const Spacer(),
          ]),
        ].map(_pad).toList(),
      ),
    );
  }

  static Widget _pad(Widget child) =>
      Padding(padding: const EdgeInsets.symmetric(vertical: 16), child: child);

  DropdownMenuItem<FindReply_ImageInfo> _buildDropdownItem(
    FindReply_ImageInfo info,
  ) =>
      DropdownMenuItem(
        value: info,
        child: Container(
          width: double.infinity,
          decoration: const BoxDecoration(
            border: Border(top: BorderSide(color: Colors.black26)),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                imageName(info.aliasesInfo.first),
                style: const TextStyle(fontSize: 20),
              ),
              Text.rich(TextSpan(children: [
                const TextSpan(
                  text: 'Version: ',
                  style: TextStyle(fontWeight: FontWeight.bold),
                ),
                TextSpan(text: info.version),
              ])),
              if (info.aliasesInfo.length > 1)
                Text.rich(TextSpan(children: [
                  const TextSpan(
                    text: 'Aliases: ',
                    style: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  TextSpan(
                    text: info.aliasesInfo.slice(1).map(imageName).join(', '),
                  ),
                ])),
              Text.rich(TextSpan(children: [
                const TextSpan(
                  text: 'Description: ',
                  style: TextStyle(fontWeight: FontWeight.bold),
                ),
                TextSpan(text: info.release),
              ])),
            ],
          ),
        ),
      );
}

String imageName(FindReply_AliasInfo info) {
  return info.remoteName.isEmpty
      ? info.alias
      : '${info.remoteName}:${info.alias}';
}

FormFieldValidator<String> _nameValidator(Iterable<String> existingNames) {
  return (String? value) {
    if (value!.isEmpty) {
      return null;
    }
    if (value.length < 2) {
      return 'Name must be at least 2 characters';
    }
    if (RegExp(r'[^A-Za-z0-9\-]').hasMatch(value)) {
      return 'Name must contain only letters, numbers and dashes';
    }
    if (RegExp(r'^[^A-Za-z]').hasMatch(value)) {
      return 'Name must start with a letter';
    }
    if (RegExp(r'[^A-Za-z0-9]$').hasMatch(value)) {
      return 'Name must end in digit or letter';
    }
    if (existingNames.contains(value)) {
      return 'Name is already in use';
    }
    return null;
  };
}

String? _memorySizeValidator(String? value) {
  return RegExp(r'^\s*\d+(\.\d+)?([KMGT]?B)?\s*$', caseSensitive: false)
          .hasMatch(value!)
      ? null
      : 'Invalid memory size';
}
