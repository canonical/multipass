import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';
import 'package:rxdart/rxdart.dart';

import '../providers.dart';
import 'launch_panel.dart';

class LaunchOperationProgress extends ConsumerWidget {
  final Stream<Either<LaunchReply, MountReply>> stream;
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
        stream: stream.doOnData((r) {
          if (r.getLeft().toNullable()?.whichCreateOneof() ==
              LaunchReply_CreateOneof.vmInstanceName) {
            Scaffold.of(context).closeEndDrawer();
            ref.invalidate(launchOperationProvider);
          }
        }),
        builder: (_, snapshot) {
          if (snapshot.hasError) {
            return Text(snapshot.error.toString());
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
                  return ('Launching $name...', null);
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
