import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/vm_details/ip_addresses.dart';

void main() {
  Widget buildWidget(List<String> ips) {
    return MaterialApp(
      locale: const Locale('en'),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(
        body: Row(children: [
          Expanded(child: IpAddresses(ips)),
        ]),
      ),
    );
  }

  group('IpAddresses', () {
    testWidgets('renders "-" when ips is empty', (tester) async {
      await tester.pumpWidget(buildWidget([]));
      await tester.pumpAndSettle();

      expect(find.text('-'), findsOneWidget);
    });

    testWidgets('renders the IP text when ips has one entry', (tester) async {
      await tester.pumpWidget(buildWidget(['192.168.1.1']));
      await tester.pumpAndSettle();

      expect(find.text('192.168.1.1'), findsOneWidget);
    });

    testWidgets('does not render a PopupMenuButton when ips has one entry',
        (tester) async {
      await tester.pumpWidget(buildWidget(['192.168.1.1']));
      await tester.pumpAndSettle();

      expect(find.byType(PopupMenuButton<void>), findsNothing);
    });

    testWidgets('does not render a Badge when ips has one entry',
        (tester) async {
      await tester.pumpWidget(buildWidget(['192.168.1.1']));
      await tester.pumpAndSettle();

      expect(find.byType(Badge), findsNothing);
    });

    testWidgets(
        'renders first IP in CopyableText and a PopupMenuButton when ips has two entries',
        (tester) async {
      await tester.pumpWidget(buildWidget(['10.0.0.1', '10.0.0.2']));
      await tester.pumpAndSettle();

      expect(find.text('10.0.0.1'), findsOneWidget);
      expect(find.byType(PopupMenuButton<void>), findsOneWidget);
    });

    testWidgets('badge count is 2 when ips has three entries', (tester) async {
      await tester
          .pumpWidget(buildWidget(['10.0.0.1', '10.0.0.2', '10.0.0.3']));
      await tester.pumpAndSettle();

      expect(find.byType(Badge), findsWidgets);
      final badge = tester.widget<Badge>(find.byType(Badge).first);
      expect(badge.label, isNotNull);
      // Badge.count with count=2 renders "2" as its label text
      expect(find.text('2'), findsOneWidget);
    });

    testWidgets(
        'tapping the popup button opens a menu showing the second IP when ips has two entries',
        (tester) async {
      await tester.pumpWidget(buildWidget(['10.0.0.1', '10.0.0.2']));
      await tester.pumpAndSettle();

      await tester.tap(find.byType(PopupMenuButton<void>));
      await tester.pumpAndSettle();

      expect(find.text('10.0.0.2'), findsOneWidget);
    });
  });
}
