import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:grpc/grpc.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/vm_table/no_vms.dart';
import 'package:multipass_gui/vm_table/vm_table_screen.dart';
import 'package:multipass_gui/vm_table/vms.dart';

import '../helpers.dart';

class _FakeGrpcClient extends GrpcClient {
  _FakeGrpcClient() : super(RpcClient(ClientChannel('localhost')));
}

class _StaticVmInfosNotifier extends AllVmInfosNotifier {
  _StaticVmInfosNotifier(this._vms);
  final List<DetailedInfoItem> _vms;

  @override
  List<DetailedInfoItem> build() => _vms;
}

Widget _buildScreen({List<DetailedInfoItem> vms = const []}) {
  return withFakeSvgAssetBundle(
    ProviderScope(
      overrides: [
        grpcClientProvider.overrideWithValue(_FakeGrpcClient()),
        allVmInfosProvider.overrideWith(() => _StaticVmInfosNotifier(vms)),
      ],
      child: MaterialApp(
        locale: const Locale('en'),
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: const VmTableScreen(),
      ),
    ),
  );
}

void main() {
  group('VmTableScreen', () {
    testWidgets('shows NoVms widget when there are no VMs', (tester) async {
      await tester.pumpWidget(_buildScreen());
      await tester.pump();

      expect(find.byType(NoVms), findsOneWidget);
      expect(find.byType(Vms), findsNothing);
    });

    testWidgets('shows Vms widget when at least one VM exists', (tester) async {
      // Suppress layout overflow from the HeaderSelection widget's fixed-width Row
      final originalOnError = FlutterError.onError;
      FlutterError.onError = (details) {
        if (details.exception.toString().contains('A RenderFlex overflowed')) {
          return;
        }
        originalOnError?.call(details);
      };
      addTearDown(() => FlutterError.onError = originalOnError);

      final vm = DetailedInfoItem(
        name: 'test-vm',
        instanceStatus: InstanceStatus(status: Status.RUNNING),
      );
      await tester.pumpWidget(_buildScreen(vms: [vm]));
      await tester.pump();

      expect(find.byType(Vms), findsOneWidget);
      expect(find.byType(NoVms), findsNothing);
    });

    testWidgets('shows NoVms widget when only DELETED VMs exist',
        (tester) async {
      final vm = DetailedInfoItem(
        name: 'deleted-vm',
        instanceStatus: InstanceStatus(status: Status.DELETED),
      );
      await tester.pumpWidget(_buildScreen(vms: [vm]));
      await tester.pump();

      expect(find.byType(NoVms), findsOneWidget);
      expect(find.byType(Vms), findsNothing);
    });
  });
}
