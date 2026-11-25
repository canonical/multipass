import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/settings/privacy_settings.dart';

final _l10n = AppLocalizationsEn();

void main() {
  Future<ProviderContainer> pump(WidgetTester tester) async {
    final container = ProviderContainer();
    addTearDown(container.dispose);
    await tester.pumpWidget(UncontrolledProviderScope(
      container: container,
      child: MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: const Scaffold(
          body: SizedBox(width: 900, child: PrivacySettings()),
        ),
      ),
    ));
    return container;
  }

  testWidgets('starts with unknown consent', (tester) async {
    final container = await pump(tester);

    expect(container.read(telemetryConsentProvider), isNull);
    final button = tester.widget<SegmentedButton<bool>>(
      find.byType(SegmentedButton<bool>),
    );
    expect(button.selected, isEmpty);
  });

  testWidgets('selecting options sets consent, re-tapping is a no-op',
      (tester) async {
    final container = await pump(tester);

    await tester.tap(find.text(_l10n.privacyTelemetryOn));
    await tester.pump();
    expect(container.read(telemetryConsentProvider), isTrue);

    await tester.tap(find.text(_l10n.privacyTelemetryOn));
    await tester.pump();
    expect(container.read(telemetryConsentProvider), isTrue);

    await tester.tap(find.text(_l10n.privacyTelemetryOff));
    await tester.pump();
    expect(container.read(telemetryConsentProvider), isFalse);
  });
}
