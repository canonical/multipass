import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/copyable_text.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/vm_details/ip_addresses.dart';

final _l10n = AppLocalizationsEn();

Widget _buildApp(Iterable<String> ips) {
  return MaterialApp(
    localizationsDelegates: AppLocalizations.localizationsDelegates,
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: IpAddresses(ips)),
  );
}

final _menuButton = find.byType(PopupMenuButton<void>);

void main() {
  group('IpAddresses', () {
    testWidgets(
        'shows an uncopyable "-" without a menu when there are no '
        'addresses', (tester) async {
      await tester.pumpWidget(_buildApp([]));

      expect(find.text('-'), findsOneWidget);
      expect(
        find.descendant(
          of: find.byType(CopyableText),
          matching: find.byType(GestureDetector),
        ),
        findsNothing,
      );
      expect(find.byType(Badge), findsNothing);
      expect(_menuButton, findsNothing);
    });

    testWidgets('shows a single address as copyable text without a menu',
        (tester) async {
      await tester.pumpWidget(_buildApp(['10.0.0.1']));

      expect(find.widgetWithText(CopyableText, '10.0.0.1'), findsOneWidget);
      expect(find.byType(Badge), findsNothing);
      expect(_menuButton, findsNothing);
    });

    for (final ips in [
      ['10.0.0.1', '10.0.0.2'],
      ['10.0.0.1', '10.0.0.2', '10.0.0.3'],
    ]) {
      testWidgets(
          'shows the first address and badges the menu with '
          '${ips.length - 1} more', (tester) async {
        await tester.pumpWidget(_buildApp(ips));

        expect(find.widgetWithText(CopyableText, ips.first), findsOneWidget);
        expect(
          find.descendant(
            of: find.byType(Badge),
            matching: find.text('${ips.length - 1}'),
          ),
          findsOneWidget,
        );
        expect(
          find.descendant(of: find.byType(Badge), matching: _menuButton),
          findsOneWidget,
        );
      });
    }

    testWidgets('lists only the other addresses under a disabled title',
        (tester) async {
      await tester.pumpWidget(_buildApp(['10.0.0.1', '10.0.0.2', '10.0.0.3']));

      await tester.tap(_menuButton);
      await tester.pumpAndSettle();

      final items = tester
          .widgetList<PopupMenuItem<void>>(find.byType(PopupMenuItem<void>))
          .toList();
      expect(
        items.map((item) => (item.child! as Text).data),
        [_l10n.ipAddressesOtherTitle, '10.0.0.2', '10.0.0.3'],
      );
      expect(items.map((item) => item.enabled), [false, true, true]);
    });
  });
}
