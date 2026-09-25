// TODO hyperv migration, remove (whole file)

import 'package:flutter/material.dart';
import 'package:grpc/grpc.dart' hide ConnectionState;

import '../extensions.dart';
import '../grpc_client.dart';
import '../l10n/app_localizations.dart';
import 'notification_entries.dart';

/// The daemon's error when the driver changed but not every instance migrated.
const _partialMigrationError = 'Driver change succeeded';

class MigrationProgress {
  final String phase;
  final List<String> diagnostics;
  final String summary;
  final bool done;
  final String? error;

  const MigrationProgress({
    this.phase = '',
    this.diagnostics = const [],
    this.summary = '',
    this.done = false,
    this.error,
  });

  MigrationProgress update(SetReply reply) {
    final report = reply.hcsMigrationReport;
    return MigrationProgress(
      phase: report.phase.isNotEmpty ? report.phase : phase,
      diagnostics: reply.logLine.trim().isNotEmpty
          ? [...diagnostics, reply.logLine.trim()]
          : diagnostics,
      summary: report.summary.isNotEmpty ? report.summary.trim() : summary,
    );
  }

  MigrationProgress finish({String? error}) => MigrationProgress(
        phase: phase,
        diagnostics: diagnostics,
        summary: summary,
        done: true,
        error: error,
      );
}

/// Folds the replies of a driver change into its migration progress, ending
/// with a finished state (errors included, so that the diagnostics gathered so
/// far aren't lost).
Stream<MigrationProgress> migrationProgress(Stream<SetReply> replies) async* {
  var progress = const MigrationProgress();
  try {
    await for (final reply in replies) {
      progress = progress.update(reply);
      yield progress;
    }
    yield progress.finish();
  } catch (error) {
    final message = error is GrpcError ? error.message : null;
    yield progress.finish(error: message ?? error.toString());
  }
}

class DriverMigrationNotification extends StatelessWidget {
  final Stream<MigrationProgress> progress;

  const DriverMigrationNotification({super.key, required this.progress});

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    return StreamBuilder<MigrationProgress>(
      stream: progress,
      builder: (_, snapshot) {
        final progress = snapshot.data ?? const MigrationProgress();

        if (!progress.done) {
          return SimpleNotification(
            barColor: Colors.blue,
            closeable: false,
            icon: const CircularProgressIndicator(
              color: Colors.blue,
              strokeAlign: -2,
              strokeWidth: 3.5,
            ),
            child: Text.rich(
              [l10n.migrationInProgress.span.bold, progress.phase.span].spans,
            ),
          );
        }

        final details = [
          ...progress.diagnostics,
          if (progress.summary.isNotEmpty) progress.summary,
        ].join('\n');
        final error = progress.error;

        if (error == null) {
          // Not a timed notification: the summary warns about running the
          // retained originals.
          return SimpleNotification(
            barColor: Colors.green,
            icon: const Icon(Icons.check_circle_outline, color: Colors.green),
            child: Text.rich(
              [l10n.migrationDone.span.bold, details.span].spans,
            ),
          );
        }

        if (error.startsWith(_partialMigrationError)) {
          return WarningNotification(
            child: Text.rich(
              [l10n.migrationDoneWithProblems.span.bold, details.span].spans,
            ),
          );
        }

        return ErrorNotification(text: l10n.virtualizationDriverError(error));
      },
    );
  }
}
