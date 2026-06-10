import 'package:flutter/material.dart' hide Switch;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/grpc_client.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/settings/autostart_notifiers.dart';
import 'package:multipass_gui/settings/general_settings.dart';
import 'package:multipass_gui/update_available.dart';

Widget _buildApp({
  UpdateInfo? update,
  String? onAppClose,
}) {
  return ProviderScope(
    overrides: [
      updateProvider.overrideWithBuild(
        (ref, _) => update ?? UpdateInfo(),
      ),
      onAppCloseProvider.overrideWithBuild(
        (ref, _) => onAppClose ?? 'ask',
      ),
      autostartProvider.overrideWith(
        () => _StaticAutostartNotifier(false),
      ),
    ],
    child: MaterialApp(
      locale: const Locale('en'),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const Scaffold(
        body: SizedBox(width: 900, child: GeneralSettings()),
      ),
    ),
  );
}

class _StaticAutostartNotifier extends AutostartNotifier {
  _StaticAutostartNotifier(this._value);
  final bool _value;

  @override
  Future<bool> build() async => _value;

  @override
  Future<void> doSet(bool value) async {}
}

void main() {
  group('GeneralSettings — UpdateAvailable banner', () {
    testWidgets('shows UpdateAvailable widget when version is set',
        (tester) async {
      await tester.pumpWidget(
        _buildApp(update: UpdateInfo()..version = '1.2.3'),
      );
      await tester.pumpAndSettle();

      expect(find.byType(UpdateAvailable), findsOneWidget);
    });

    testWidgets('hides UpdateAvailable widget when version is blank',
        (tester) async {
      await tester.pumpWidget(_buildApp(update: UpdateInfo()));
      await tester.pumpAndSettle();

      expect(find.byType(UpdateAvailable), findsNothing);
    });
  });

  group('GeneralSettings — on-close dropdown', () {
    testWidgets('renders on-close dropdown', (tester) async {
      await tester.pumpWidget(_buildApp(onAppClose: 'ask'));
      await tester.pumpAndSettle();

      expect(find.byType(DropdownButton<String>), findsOneWidget);
    });
  });
}
