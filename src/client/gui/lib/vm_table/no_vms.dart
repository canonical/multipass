import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../catalogue/catalogue.dart';
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
              TextSpan(
                style: const TextStyle(color: Colors.black, fontSize: 16),
                children: [
                  TextSpan(text: l10n.noVmsMessageBefore),
                  WidgetSpan(
                    alignment: PlaceholderAlignment.baseline,
                    baseline: TextBaseline.alphabetic,
                    child: GestureDetector(
                      key: const Key('catalogue_link'),
                      onTap: goToCatalogue,
                      child: MouseRegion(
                        cursor: SystemMouseCursors.click,
                        child: Text(
                          l10n.catalogueLabel,
                          style: const TextStyle(
                            color: Colors.blue,
                            fontSize: 16,
                          ),
                        ),
                      ),
                    ),
                  ),
                  TextSpan(text: l10n.noVmsMessageAfter),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
