import 'package:fixnum/fixnum.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:multipass_gui/providers.dart';

import 'helpers/app_harness.dart';
import 'helpers/mock_daemon.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Launch form: name validation and mount point add/remove',
      (tester) async {
    const existingVm = 'existing-vm';

    final mockDaemon = MockDaemon.nice();

    // Info includes an existing VM so nameValidator can detect "in use".
    mockDaemon.onInfo = (_) => Stream.value(
          InfoReply(
            details: [
              DetailedInfoItem(
                name: existingVm,
                instanceStatus: InstanceStatus(status: Status.RUNNING),
              ),
            ],
          ),
        );

    // Provide host resources so RamSlider/DiskSlider have min < max.
    mockDaemon.onDaemonInfo = (_) => Stream.value(
          DaemonInfoReply(
            cpus: 8,
            memory: Int64(8 * 1024 * 1024 * 1024),
            availableSpace: Int64(50 * 1024 * 1024 * 1024),
          ),
        );

    // Needed when the VM details page loads.
    mockDaemon.onGet = (_) => Stream.value(GetReply(value: ''));
    mockDaemon.onNetworks = (_) => Stream.value(NetworksReply());
    mockDaemon.onVersion = (_) => Stream.value(VersionReply());
    mockDaemon.onSshInfo = (_) => Stream.value(SSHInfoReply());

    // Return a single Ubuntu image for the catalogue.
    mockDaemon.enqueueFind((_) => Stream.value(
          FindReply(
            imagesInfo: [
              FindReply_ImageInfo(
                os: 'Ubuntu',
                release: '22.04',
                codename: 'jammy',
                aliases: ['lts', '22.04'],
              ),
            ],
          ),
        ));

    await mockDaemon.serve();
    addTearDown(mockDaemon.shutdown);

    await launchApp(tester, [
      ffiAvailableProvider.overrideWithValue(false),
      grpcClientProvider.overrideWithValue(mockDaemon.client),
    ]);
    await tester.pumpAndSettle();

    // Catalogue is shown by default. Wait for find reply to load.
    await pumpUntil(tester, find.text('Ubuntu Server'));

    expect(find.text('Ubuntu Server'), findsOneWidget);

    // Tap Configure on the first image card to open the launch form drawer.
    await tester.tap(find.text('Configure').first);
    await tester.pumpAndSettle();

    // Launch form ("Configure instance") is now visible.
    expect(find.text('Configure instance'), findsOneWidget);

    // --- Name field validation ---
    // The first TextFormField in the form is the instance name input.
    final nameField = find.byType(TextFormField).first;

    // Single character — "Name must be at least two characters".
    await tester.enterText(nameField, 'a');
    await tester.pumpAndSettle();
    expect(find.text('Name must be at least two characters'), findsOneWidget);

    // Starts with a digit — "Name must start with a letter".
    await tester.enterText(nameField, '1a');
    await tester.pumpAndSettle();
    expect(find.text('Name must start with a letter'), findsOneWidget);

    // Ends with a dash — "Name must end in digit or letter".
    await tester.enterText(nameField, 'ab-');
    await tester.pumpAndSettle();
    expect(find.text('Name must end in digit or letter'), findsOneWidget);

    // Contains invalid character — "Name must contain only letters, numbers and dashes".
    await tester.enterText(nameField, 'ab!');
    await tester.pumpAndSettle();
    expect(
      find.text('Name must contain only letters, numbers and dashes'),
      findsOneWidget,
    );

    // Name already in use — "Name is already in use".
    await tester.enterText(nameField, existingVm);
    await tester.pumpAndSettle();
    expect(find.text('Name is already in use'), findsOneWidget);

    // Empty name is valid (uses random hint name) — no error.
    await tester.enterText(nameField, '');
    await tester.pumpAndSettle();
    expect(find.text('Name must be at least two characters'), findsNothing);
    expect(find.text('Name must start with a letter'), findsNothing);
    expect(find.text('Name is already in use'), findsNothing);

    // Valid name — no error.
    await tester.enterText(nameField, 'my-instance');
    await tester.pumpAndSettle();
    expect(find.text('Name must be at least two characters'), findsNothing);
    expect(find.text('Name must start with a letter'), findsNothing);
    expect(find.text('Name is already in use'), findsNothing);

    // --- Resources section (CPU/RAM/disk sliders) ---
    expect(find.text('Resources'), findsOneWidget);
    // Three Slider widgets are present (CPUs, RAM, disk).
    expect(find.byType(Slider), findsNWidgets(3));

    // --- Mount point add/remove ---
    // Scroll down to make the Mounts section visible.
    await tester.ensureVisible(find.text('Add mount'));
    await tester.pumpAndSettle();

    // Tap "Add mount" to open the mount entry form.
    await tester.tap(find.text('Add mount'));
    await tester.pumpAndSettle();

    // Mount form controls are visible (Save + Cancel row).
    expect(find.text('Save'), findsOneWidget);
    expect(find.text('Cancel'), findsAtLeastNWidgets(1));

    // Cancel removes the mount form and restores the "Add mount" button.
    await tester.tap(find.text('Cancel').first);
    await tester.pumpAndSettle();

    expect(find.text('Save'), findsNothing);
    expect(find.text('Add mount'), findsOneWidget);

    // Set up the launch handler before submitting the form.
    LaunchRequest? capturedLaunchReq;
    mockDaemon.enqueueLaunch((req) {
      capturedLaunchReq = req;
      mockDaemon.onInfo = (_) => Stream.value(
            InfoReply(
              details: [
                DetailedInfoItem(
                  name: req.instanceName,
                  instanceStatus: InstanceStatus(status: Status.RUNNING),
                  memoryTotal: '${2 * 1024 * 1024 * 1024}',
                  diskTotal: '${20 * 1024 * 1024 * 1024}',
                  instanceInfo: InstanceDetails(
                    memoryUsage: '${512 * 1024 * 1024}',
                    diskUsage: '${2 * 1024 * 1024 * 1024}',
                  ),
                ),
              ],
            ),
          );
      return Stream.value(LaunchReply());
    });

    // Submit the configure form.
    await tester.tap(find.text('Launch').last);
    await pumpUntil(tester, find.text('1'));

    expect(capturedLaunchReq?.instanceName, equals('my-instance'));

    // Navigate to Instances and open the launched VM's details.
    await tester.tap(find.textContaining('Instances'));
    await pumpUntil(tester, find.byTooltip('my-instance'));

    await tester.tap(find.byTooltip('my-instance'));
    await tester.pumpAndSettle();

    // Switch to Details tab and verify the Resources section.
    await tester.tap(find.text('Details'));
    await pumpUntil(tester, find.text('Resources'));

    expect(find.text('Resources'), findsOneWidget);

    // Header stat labels are visible.
    expect(find.text('MEMORY USAGE'), findsOneWidget);
    expect(find.text('DISK USAGE'), findsOneWidget);

    // Formatted usage values from the Info reply.
    expect(find.textContaining('512.0MiB / 2.0GiB'), findsOneWidget);
    expect(find.textContaining('2.0GiB / 20.0GiB'), findsOneWidget);

    mockDaemon.assertAllConsumed();
  });
}
