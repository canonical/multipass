import 'dart:async';

import 'package:built_collection/built_collection.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:fpdart/fpdart.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/notifications/notification_entries.dart';
import 'package:multipass_gui/notifications/notifications_list.dart';
import 'package:multipass_gui/notifications/notifications_provider.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/sidebar.dart';

final _l10n = AppLocalizationsEn();

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

    testWidgets('close button dispatches CloseNotificationIntent',
        (tester) async {
      var invoked = false;

      await tester.pumpWidget(
        buildWidget(
          Actions(
            actions: {
              CloseNotificationIntent: CallbackAction<CloseNotificationIntent>(
                onInvoke: (_) => invoked = true,
              ),
            },
            child: SimpleNotification(
              barColor: Colors.blue,
              icon: const Icon(Icons.info),
              child: const Text('close me'),
            ),
          ),
        ),
      );

      await tester.tap(find.byIcon(Icons.close));
      expect(invoked, isTrue);
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

  group('TimeoutNotification', () {
    Future<void> pumpTimeout(WidgetTester tester, VoidCallback onClose) {
      return tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Actions(
              actions: {
                CloseNotificationIntent:
                    CallbackAction<CloseNotificationIntent>(
                  onInvoke: (_) => onClose(),
                ),
              },
              child: Align(
                alignment: Alignment.topLeft,
                child: SizedBox(
                  width: 300,
                  height: 60,
                  child: TimeoutNotification(
                    barColor: Colors.green,
                    icon: const Icon(Icons.check),
                    duration: const Duration(seconds: 1),
                    child: const Text('will close'),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
    }

    testWidgets('closes only after the duration elapses', (tester) async {
      var closed = false;
      await pumpTimeout(tester, () => closed = true);

      await tester.pump(const Duration(milliseconds: 900));
      expect(closed, isFalse);

      await tester.pump(const Duration(milliseconds: 200));
      expect(closed, isTrue);
    });

    testWidgets('hovering pauses the timeout and leaving restarts it',
        (tester) async {
      var closed = false;
      final mouse = await tester.createGesture(kind: PointerDeviceKind.mouse);
      await mouse.addPointer(location: const Offset(700, 500));
      addTearDown(mouse.removePointer);
      await pumpTimeout(tester, () => closed = true);

      await tester.pump(const Duration(milliseconds: 900));
      await mouse.moveTo(tester.getCenter(find.text('will close')));
      await tester.pump(const Duration(seconds: 2));
      expect(closed, isFalse);

      await mouse.moveTo(const Offset(700, 500));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 900));
      expect(closed, isFalse);

      await tester.pump(const Duration(milliseconds: 200));
      expect(closed, isTrue);
    });
  });

  group('SuccessNotification', () {
    testWidgets('shows the child with a success icon', (tester) async {
      await tester.pumpWidget(
        buildWidget(
          const SuccessNotification(child: Text('success message')),
        ),
      );

      expect(find.text('success message'), findsOneWidget);
      expect(find.byIcon(Icons.check_circle_outline), findsOneWidget);
    });
  });

  group('LaunchingNotification', () {
    Future<ProviderContainer> pumpApp(WidgetTester tester, Widget body) async {
      final container = ProviderContainer(
        overrides: [
          vmNamesProvider.overrideWith((ref) => BuiltSet<String>()),
        ],
      );
      addTearDown(container.dispose);
      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: container,
          child: MaterialApp(
            localizationsDelegates: AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(body: body),
          ),
        ),
      );
      return container;
    }

    StreamController<Either<LaunchReply, MountReply>?> newController() {
      final controller =
          StreamController<Either<LaunchReply, MountReply>?>.broadcast();
      addTearDown(controller.close);
      return controller;
    }

    Either<LaunchReply, MountReply> progress(
      LaunchProgress_ProgressType type, [
      String percentComplete = '',
    ]) {
      return Left(LaunchReply(
        launchProgress: LaunchProgress(
          type: type,
          percentComplete: percentComplete,
        ),
      ));
    }

    testWidgets('shows GrpcError.message text on stream error', (tester) async {
      final controller = newController();
      await pumpApp(
        tester,
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );
      await tester.pump();

      final error =
          GrpcError.custom(StatusCode.unknown, 'gRPC failure message');
      controller.addError(error);
      await tester.pump();

      expect(find.text('gRPC failure message'), findsOneWidget);
    });

    testWidgets('shows error.toString() for non-GrpcError on stream error',
        (tester) async {
      final controller = newController();
      await pumpApp(
        tester,
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );
      await tester.pump();

      controller.addError(Exception('plain exception'));
      await tester.pump();

      expect(find.textContaining('plain exception'), findsOneWidget);
    });

    testWidgets(
        'shows SuccessNotification with "Go to instance" when stream completes',
        (tester) async {
      await pumpApp(
        tester,
        LaunchingNotification(
          stream: Stream<Either<LaunchReply, MountReply>?>.fromIterable([]),
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );
      await tester.pump();

      expect(find.byType(SuccessNotification), findsOneWidget);
      expect(find.text(_l10n.launchGoToInstance), findsOneWidget);
    });

    testWidgets(
        '"Go to instance" button sets sidebarKeyProvider to "vm-{name}"',
        (tester) async {
      final container = await pumpApp(
        tester,
        LaunchingNotification(
          stream: Stream<Either<LaunchReply, MountReply>?>.fromIterable([]),
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );
      await tester.pump();

      await tester.tap(find.text(_l10n.launchGoToInstance));
      expect(container.read(sidebarKeyProvider), equals('vm-my-vm'));
    });

    testWidgets('tapping the in-progress notification goes to the instance',
        (tester) async {
      final controller = newController();
      final container = await pumpApp(
        tester,
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );

      controller.add(progress(LaunchProgress_ProgressType.VERIFY));
      await tester.pump();

      await tester.tap(find.textContaining(_l10n.launchVerifyingImage));
      expect(container.read(sidebarKeyProvider), equals('vm-my-vm'));
    });

    testWidgets('VERIFY progress shows verify message without cancel button',
        (tester) async {
      final controller = newController();
      await pumpApp(
        tester,
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );

      controller.add(progress(LaunchProgress_ProgressType.VERIFY));
      await tester.pump();

      expect(find.textContaining(_l10n.launchVerifyingImage), findsOneWidget);
      expect(find.text(_l10n.commonCancel), findsNothing);
    });

    testWidgets('download progress shows percentage and cancel button',
        (tester) async {
      final controller = newController();
      await pumpApp(
        tester,
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: Completer(),
          name: 'my-vm',
        ),
      );

      controller.add(progress(LaunchProgress_ProgressType.IMAGE, '42'));
      await tester.pump();

      expect(find.textContaining('42'), findsOneWidget);
      expect(find.text(_l10n.commonCancel), findsOneWidget);
    });

    final messageReplies = <String, Either<LaunchReply, MountReply>>{
      'create message': Left(LaunchReply(createMessage: 'status update')),
      'launch reply message': Left(LaunchReply(replyMessage: 'status update')),
      'mount reply message': Right(MountReply(replyMessage: 'status update')),
    };

    for (final MapEntry(key: kind, value: reply) in messageReplies.entries) {
      testWidgets('shows the $kind without a cancel button', (tester) async {
        final controller = newController();
        await pumpApp(
          tester,
          LaunchingNotification(
            stream: controller.stream,
            cancelCompleter: Completer(),
            name: 'my-vm',
          ),
        );

        controller.add(reply);
        await tester.pump();

        expect(find.textContaining('status update'), findsOneWidget);
        expect(find.text(_l10n.commonCancel), findsNothing);
      });
    }

    testWidgets('tapping cancel button completes cancelCompleter',
        (tester) async {
      final cancelCompleter = Completer<void>();
      final controller = newController();
      await pumpApp(
        tester,
        LaunchingNotification(
          stream: controller.stream,
          cancelCompleter: cancelCompleter,
          name: 'my-vm',
        ),
      );

      controller.add(progress(LaunchProgress_ProgressType.IMAGE, '50'));
      await tester.pump();

      await tester.tap(find.text(_l10n.commonCancel));
      await tester.pump();

      expect(cancelCompleter.isCompleted, isTrue);
    });

    testWidgets('tapping cancel again while the notification closes is safe',
        (tester) async {
      final controller = newController();
      final container = await pumpApp(tester, const NotificationList());
      container.read(notificationsProvider.notifier).add(
            LaunchingNotification(
              stream: controller.stream,
              cancelCompleter: Completer(),
              name: 'my-vm',
            ),
          );
      await tester.pump();
      controller.add(progress(LaunchProgress_ProgressType.IMAGE, '50'));
      await tester.pump();
      await tester.pump(const Duration(seconds: 1));

      final cancel = find.text(_l10n.commonCancel);
      await tester.tap(cancel);
      await tester.pump(const Duration(milliseconds: 50));
      await tester.tap(cancel);
      await tester.pumpAndSettle();

      expect(tester.takeException(), isNull);
      expect(cancel, findsNothing);
    });
  });
}
