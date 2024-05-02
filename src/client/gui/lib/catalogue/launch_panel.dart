import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fpdart/fpdart.dart';

import '../providers.dart';
import 'launch_form.dart';
import 'launch_operation_progress.dart';

class LaunchOperation {
  final Stream<Either<LaunchReply, MountReply>?> stream;
  final String name;
  final String image;

  LaunchOperation({
    required this.stream,
    required this.name,
    required this.image,
  });
}

final launchOperationProvider = StateProvider<LaunchOperation?>((_) => null);

class LaunchPanel extends ConsumerWidget {
  const LaunchPanel({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final values = ref.watch(launchOperationProvider);
    if (values == null) {
      return LaunchForm();
    }

    final LaunchOperation(:stream, :name, :image) = values;
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
