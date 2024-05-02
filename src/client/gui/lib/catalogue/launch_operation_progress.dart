import 'dart:async';

import 'package:basics/basics.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';
import 'package:grpc/grpc.dart';
import 'package:rxdart/rxdart.dart';

import '../providers.dart';
import '../sidebar.dart';
import 'launch_panel.dart';

class LaunchOperationProgress extends ConsumerWidget {
  final Stream<Either<LaunchReply, MountReply>?> stream;
  final String name;
  final String image;

  const LaunchOperationProgress({
    super.key,
    required this.stream,
    required this.name,
    required this.image,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Container(
      color: Colors.white,
      padding: const EdgeInsets.all(16),
      alignment: Alignment.topLeft,
      child: StreamBuilder(
        stream: stream.doOnData((reply) {
          if (reply != null) return;
          Scaffold.of(context).closeEndDrawer();
          Timer(250.milliseconds, () {
            ref.invalidate(launchOperationProvider);
            ref.read(sidebarKeyProvider.notifier).state = 'vm-$name';
          });
        }),
        builder: (_, snapshot) {
          if (snapshot.hasError) {
            final error = snapshot.error;
            final errorMessage =
                error is GrpcError ? error.message : error.toString();

            return Row(children: [
              Expanded(child: Text(errorMessage ?? error.toString())),
              Container(
                height: 150,
                alignment: Alignment.topCenter,
                child: IconButton(
                  icon: const Icon(Icons.close),
                  onPressed: () {
                    Scaffold.of(context).closeEndDrawer();
                    Timer(250.milliseconds, () {
                      ref.invalidate(launchOperationProvider);
                    });
                  },
                ),
              ),
            ]);
          }

          if (!snapshot.hasData) return const SizedBox.expand();

          final (message, downloaded) = snapshot.data!.match(
            (l) {
              switch (l.whichCreateOneof()) {
                case LaunchReply_CreateOneof.launchProgress:
                  if (l.launchProgress.type ==
                      LaunchProgress_ProgressTypes.VERIFY) {
                    return ('Verifying image', null);
                  }
                  return (
                    'Downloading image',
                    int.tryParse(l.launchProgress.percentComplete),
                  );
                case LaunchReply_CreateOneof.createMessage:
                  return (l.createMessage, null);
                default:
                  return (
                    l.replyMessage.isBlank
                        ? 'Launching $name...'
                        : l.replyMessage,
                    null
                  );
              }
            },
            (m) => (m.replyMessage, null),
          );

          final (downloadedDecimal, downloadedText) = downloaded == null
              ? (null, '')
              : (downloaded.toDouble() / 100, '$downloaded%');

          return Row(children: [
            Text(name, style: const TextStyle(fontSize: 37)),
            const SizedBox(width: 20, height: 150),
            Padding(
              padding: const EdgeInsets.all(8.0),
              child: Stack(alignment: Alignment.center, children: [
                Text(downloadedText),
                CircularProgressIndicator(value: downloadedDecimal),
              ]),
            ),
            const SizedBox(width: 10),
            Expanded(child: Text(message)),
            Text(
              image,
              style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w300),
            ),
          ]);
        },
      ),
    );
  }
}
