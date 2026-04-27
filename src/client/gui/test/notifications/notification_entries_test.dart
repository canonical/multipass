import 'dart:async';

import 'package:built_collection/built_collection.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:fpdart/fpdart.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/notifications/notification_entries.dart';
import 'package:multipass_gui/notifications/notifications_list.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/sidebar.dart';

Widget buildWidget(Widget child) {
  return MaterialApp(home: Scaffold(body: child));
}

void main() {
  group('SimpleNotification', () {
    testWidgets('renders child widget', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          SimpleNotification(
            barColor: Colors.blue,
            icon: const Icon(Icons.info),
            child: const Text('hello'),
          ),
        ),
      );
      expect(find.text('hello'), findsOneWidget);
    });

    testWidgets('shows close button when closeable is true', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          SimpleNotification(
            barColor: Colors.blue,
            icon: const Icon(Icons.info),
            closeable: true,
            child: const Text('x'),
          ),
        ),
      );
      expect(find.byIcon(Icons.close), findsOneWidget);
    });

    testWidgets('hides close button when closeable is false', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          SimpleNotification(
            barColor: Colors.blue,
            icon: const Icon(Icons.info),
            closeable: false,
            child: const Text('x'),
          ),
        ),
      );
      expect(find.byIcon(Icons.close), findsNothing);
    });

    testWidgets('renders the icon', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          SimpleNotification(
            barColor: Colors.green,
            icon: const Icon(Icons.check_circle),
            child: const Text('x'),
          ),
        ),
      );
      expect(find.byIcon(Icons.check_circle), findsOneWidget);
    });
  });

  group('ErrorNotification', () {
    testWidgets('shows error text', (tester) async {
      await tester.pumpWidget(
        buildWidget(ErrorNotification(text: 'Something went wrong')),
      );
      expect(find.text('Something went wrong'), findsOneWidget);
    });

    testWidgets('shows cancel outlined icon', (tester) async {
      await tester.pumpWidget(
        buildWidget(ErrorNotification(text: 'error')),
      );
      expect(find.byIcon(Icons.cancel_outlined), findsOneWidget);
    });

    testWidgets('is closeable (shows close button)', (tester) async {
      await tester.pumpWidget(
        buildWidget(ErrorNotification(text: 'error')),
      );
      expect(find.byIcon(Icons.close), findsOneWidget);
    });
  });

  group('OperationNotification', () {
    testWidgets('shows CircularProgressIndicator while pending',
        (tester) async {
      final completer = Completer<String>();
      await tester.pumpWidget(
        buildWidget(
          OperationNotification(
            future: completer.future,
            text: 'Working...',
          ),
        ),
      );
      await tester.pump();

      expect(find.byType(CircularProgressIndicator), findsOneWidget);
      expect(find.text('Working...'), findsOneWidget);

      completer.complete('');
    });

    testWidgets('shows success notification when future completes',
        (tester) async {
      await tester.pumpWidget(
        buildWidget(
          OperationNotification(
            future: Future.value('Done!'),
            text: 'Working...',
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Done!'), findsOneWidget);
      expect(find.byType(CircularProgressIndicator), findsNothing);
    });

    testWidgets('shows error notification when future fails', (tester) async {
      final future = Future<String>.error('Failed badly');
      future.ignore();

      await tester.pumpWidget(
        buildWidget(
          OperationNotification(
            future: future,
            text: 'Working...',
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.textContaining('Failed badly'), findsOneWidget);
      expect(find.byIcon(Icons.cancel_outlined), findsOneWidget);
    });
  });

  group('SimpleNotification close button', () {
    testWidgets('tapping close button dispatches CloseNotificationIntent',
        (tester) async {
      var invoked = false;

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Actions(
              actions: {
                CloseNotificationIntent:
                    CallbackAction<CloseNotificationIntent>(
                  onInvoke: (_) => invoked = true,
                ),
              },
              child: SimpleNotification(
                barColor: Colors.blue,
                icon: const Icon(Icons.info),
                closeable: true,
                child: const Text('close me'),
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.byIcon(Icons.close));
      expect(invoked, isTrue);
    });
  });

  group('TimeoutNotification', () {
    testWidgets('renders child widget', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          TimeoutNotification(
            barColor: Colors.green,
            icon: const Icon(Icons.check),
            child: const Text('timeout content'),
          ),
        ),
      );
      await tester.pump();

      expect(find.text('timeout content'), findsOneWidget);
    });

    testWidgets('renders icon', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          TimeoutNotification(
            barColor: Colors.green,
            icon: const Icon(Icons.check_circle),
            child: const Text('x'),
          ),
        ),
      );
      await tester.pump();

      expect(find.byIcon(Icons.check_circle), findsOneWidget);
    });

    testWidgets('auto-closes after duration via CloseNotificationIntent',
        (tester) async {
      var closed = false;

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Actions(
              actions: {
                CloseNotificationIntent:
                    CallbackAction<CloseNotificationIntent>(
                  onInvoke: (_) => closed = true,
                ),
              },
              child: TimeoutNotification(
                barColor: Colors.green,
                icon: const Icon(Icons.check),
                duration: Duration.zero,
                child: const Text('will close'),
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      expect(closed, isTrue);
    });
  });

  group('SuccessNotification', () {
    testWidgets('renders child widget', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          const SuccessNotification(child: Text('success message')),
        ),
      );
      await tester.pump();

      expect(find.text('success message'), findsOneWidget);
    });

    testWidgets('renders green check circle icon', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          const SuccessNotification(child: Text('ok')),
        ),
      );
      await tester.pump();

      expect(find.byIcon(Icons.check_circle_outline), findsOneWidget);
    });
  });

  group('LaunchingNotification', () {
    Widget buildApp(Widget child) {
      return ProviderScope(
        overrides: [
          vmNamesProvider.overrideWith((ref) => BuiltSet<String>()),
        ],
        child: MaterialApp(
          locale: const Locale('en'),
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          supportedLocales: AppLocalizations.supportedLocales,
          home: Scaffold(body: child),
        ),
      );
    }

    testWidgets('shows GrpcError.message text on stream error', (tester) async {
      final controller =
          StreamController<Either<LaunchReply, MountReply>?>.broadcast();
      addTearDown(controller.close);

      await tester.pumpWidget(buildApp(
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      ));
      await tester.pump();

      final error =
          GrpcError.custom(StatusCode.unknown, 'gRPC failure message');
      controller.addError(error);
      await tester.pump();

      expect(find.text('gRPC failure message'), findsOneWidget);
    });

    testWidgets('shows error.toString() for non-GrpcError on stream error',
        (tester) async {
      final controller =
          StreamController<Either<LaunchReply, MountReply>?>.broadcast();
      addTearDown(controller.close);

      await tester.pumpWidget(buildApp(
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      ));
      await tester.pump();

      controller.addError(Exception('plain exception'));
      await tester.pump();

      expect(find.textContaining('plain exception'), findsOneWidget);
    });

    testWidgets(
        'shows SuccessNotification with "Go to instance" when stream completes',
        (tester) async {
      await tester.pumpWidget(buildApp(
        LaunchingNotification(
          stream: Stream<Either<LaunchReply, MountReply>?>.fromIterable([]),
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      ));
      await tester.pump();

      expect(find.byType(SuccessNotification), findsOneWidget);
      expect(find.text('Go to instance'), findsOneWidget);
    });

    testWidgets(
        '"Go to instance" button sets sidebarKeyProvider to "vm-{name}"',
        (tester) async {
      final container = ProviderContainer(
        overrides: [
          vmNamesProvider.overrideWith((ref) => BuiltSet<String>()),
        ],
      );
      addTearDown(container.dispose);
      container.listen(sidebarKeyProvider, (_, __) {});

      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: container,
          child: MaterialApp(
            locale: const Locale('en'),
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(
              body: LaunchingNotification(
                stream:
                    Stream<Either<LaunchReply, MountReply>?>.fromIterable([]),
                cancelCompleter: Completer(),
                name: 'my-vm',
              ),
            ),
          ),
        ),
      );
      await tester.pump();

      await tester.tap(find.text('Go to instance'));
      expect(container.read(sidebarKeyProvider), equals('vm-my-vm'));
    });

    testWidgets('VERIFY progress shows verify message without cancel button',
        (tester) async {
      final controller =
          StreamController<Either<LaunchReply, MountReply>?>.broadcast();
      addTearDown(controller.close);

      await tester.pumpWidget(buildApp(
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      ));

      controller.add(Left(LaunchReply(
        launchProgress: LaunchProgress(
          type: LaunchProgress_ProgressTypes.VERIFY,
        ),
      )));
      await tester.pump();

      expect(find.textContaining('Verifying image'), findsOneWidget);
      expect(find.text('Cancel'), findsNothing);
    });

    testWidgets('download progress shows percentage and cancel button',
        (tester) async {
      final controller =
          StreamController<Either<LaunchReply, MountReply>?>.broadcast();
      addTearDown(controller.close);

      await tester.pumpWidget(buildApp(
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      ));

      controller.add(Left(LaunchReply(
        launchProgress: LaunchProgress(
          type: LaunchProgress_ProgressTypes.IMAGE,
          percentComplete: '42',
        ),
      )));
      await tester.pump();

      expect(find.textContaining('42'), findsOneWidget);
      expect(find.text('Cancel'), findsOneWidget);
    });

    testWidgets('tapping cancel button completes cancelCompleter',
        (tester) async {
      final cancelCompleter = Completer<void>();
      final controller =
          StreamController<Either<LaunchReply, MountReply>?>.broadcast();
      addTearDown(controller.close);

      await tester.pumpWidget(buildApp(
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: cancelCompleter,
          name: 'my-vm',
        ),
      ));

      controller.add(Left(LaunchReply(
        launchProgress: LaunchProgress(
          type: LaunchProgress_ProgressTypes.IMAGE,
          percentComplete: '50',
        ),
      )));
      await tester.pump();

      await tester.tap(find.text('Cancel'));
      await tester.pump();

      expect(cancelCompleter.isCompleted, isTrue);
    });
  });
}
