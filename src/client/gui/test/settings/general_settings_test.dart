import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/providers.dart';
import 'package:multipass_gui/settings/autostart_notifiers.dart';
import 'package:multipass_gui/settings/general_settings.dart';
import 'package:multipass_gui/update_available.dart';

final _l10n = AppLocalizationsEn();

Widget _buildApp({
  UpdateInfo? update,
  List<String>? onAppCloseSets,
  List<bool>? autostartSets,
}) {
  return ProviderScope(
    overrides: [
      updateProvider.overrideWithBuild(
        (ref, _) => update ?? UpdateInfo(),
      ),
      onAppCloseProvider.overrideWith(
        () => _FakeGuiSettingNotifier(
          onAppCloseKey,
          'ask',
          onAppCloseSets ?? [],
        ),
      ),
      autostartProvider.overrideWith(
        () => _StaticAutostartNotifier(false, autostartSets ?? []),
      ),
    ],
    child: MaterialApp(
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: const Scaffold(
        body: SizedBox(width: 900, child: GeneralSettings()),
      ),
    ),
  );
}

class _StaticAutostartNotifier extends AutostartNotifier {
  _StaticAutostartNotifier(this._value, this._setCalls);
  final bool _value;
  final List<bool> _setCalls;

  @override
  Future<bool> build() async => _value;

  @override
  Future<void> doSet(bool value) async => _setCalls.add(value);
}

class _FakeGuiSettingNotifier extends GuiSettingNotifier {
  _FakeGuiSettingNotifier(super.arg, this._value, this._setCalls);
  final String _value;
  final List<String> _setCalls;

  @override
  String build() => _value;

  @override
  void set(String value) => _setCalls.add(value);
}

void main() {
  group('GeneralSettings: UpdateAvailable banner', () {
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

  group('GeneralSettings: autostart', () {
    testWidgets('toggling the switch saves the new value', (tester) async {
      final autostartSets = <bool>[];
      await tester.pumpWidget(_buildApp(autostartSets: autostartSets));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(CupertinoSwitch));
      await tester.pumpAndSettle();

      expect(autostartSets, [true]);
    });
  });

  group('GeneralSettings: on-close dropdown', () {
    testWidgets('selecting an option saves it', (tester) async {
      final onAppCloseSets = <String>[];
      await tester.pumpWidget(_buildApp(onAppCloseSets: onAppCloseSets));
      await tester.pumpAndSettle();

      await tester.tap(find.text(_l10n.generalOnCloseAsk));
      await tester.pumpAndSettle();
      await tester.tap(find.text(_l10n.generalOnCloseStop).last);
      await tester.pumpAndSettle();

      expect(onAppCloseSets, ['stop']);
    });
  });
}
