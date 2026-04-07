import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../catalogue/catalogue.dart';
import '../extensions.dart';
import '../l10n/app_localizations.dart';
import '../sidebar.dart';

class NoVms extends ConsumerWidget {
  const NoVms({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;

    final multipassLogo = SvgPicture.asset(
      'assets/multipass.svg',
      width: 40,
      colorFilter: const ColorFilter.mode(Color(0xffD9D9D9), BlendMode.srcIn),
    );

    goToCatalogue() {
      ref.read(sidebarKeyProvider.notifier).set(CatalogueScreen.sidebarKey);
    }

    return Center(
      child: SizedBox(
        width: 400,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            multipassLogo,
            const SizedBox(height: 22),
            Text(l10n.noVmsTitle, style: const TextStyle(fontSize: 21)),
            const SizedBox(height: 8),
            Text.rich(
              [
                l10n.noVmsMessageBefore.span,
                l10n.noVmsMessageLink.span
                    .color(Colors.blue)
                    .link(ref, goToCatalogue),
                l10n.noVmsMessageAfter.span,
              ].spans.size(16),
            ),
          ],
        ),
      ),
    );
  }
}
