import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../catalogue/catalogue.dart';
import '../extensions.dart';
import '../sidebar.dart';

class NoVms extends ConsumerWidget {
  const NoVms({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final multipassLogo = SvgPicture.asset(
      'assets/multipass.svg',
      width: 40,
      colorFilter: const ColorFilter.mode(
        Color(0xffD9D9D9),
        BlendMode.srcIn,
      ),
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
            const Text('Zero Instances', style: TextStyle(fontSize: 21)),
            const SizedBox(height: 8),
            Text.rich([
              'Return to the '.span,
              'Catalogue'.span.color(Colors.blue).link(ref, goToCatalogue),
              ' to choose your instance or get started with the primary Ubuntu Image'
                  .span,
            ].spans.size(16)),
          ],
        ),
      ),
    );
  }
}
