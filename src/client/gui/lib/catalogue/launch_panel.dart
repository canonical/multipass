import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';

import '../providers.dart';
import 'launch_form.dart';
import 'launch_operation_progress.dart';

final launchOperationProvider =
    StateProvider<(Stream<Either<LaunchReply, MountReply>?>, String, String)?>(
        (_) => null);

class LaunchPanel extends ConsumerWidget {
  const LaunchPanel({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final values = ref.watch(launchOperationProvider);
    if (values == null) {
      return LaunchForm();
    }

    final (stream, name, image) = values;
    return FocusableActionDetector(
      autofocus: true,
      actions: {
        DismissIntent: CallbackAction<DismissIntent>(
          onInvoke: (_) => null,
        ),
      },
      child: LaunchOperationProgress(
        stream: stream,
        name: name,
        image: image,
      ),
    );
  }
}
