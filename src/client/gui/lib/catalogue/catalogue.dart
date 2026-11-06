import 'dart:async';
import 'dart:math';

import 'package:basics/basics.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';
import 'package:intersperse/intersperse.dart';

import '../l10n/app_localizations.dart';
import '../providers.dart';
import 'image_card.dart';
import 'launch_form.dart';

class SelectedImageNotifier extends Notifier<ImageInfo?> {
  SelectedImageNotifier(this.arg);
  final String arg;

  @override
  ImageInfo? build() {
    return null;
  }

  void set(ImageInfo image) {
    state = image;
  }
}

final selectedImageProvider =
    NotifierProvider.family<SelectedImageNotifier, ImageInfo?, String>(
  SelectedImageNotifier.new,
);

final imagesProvider = FutureProvider<List<ImageInfo>>((ref) async {
  if (!ref.watch(daemonAvailableProvider)) {
    // When daemon is not available, return empty list
    // The UI can handle this state appropriately
    return [];
  }

  final images =
      await ref.watch(grpcClientProvider).images().then((r) => r.imagesInfo);

  // artificial delay so that we can see the loading spinner a bit
  // otherwise the reply arrives too quickly and we only see a flash of the spinner
  await Future.delayed(1.seconds);

  return images;
});

List<Widget> _groupAndCreateCards(List<ImageInfo> images, double cardWidth) {
  bool isCore(ImageInfo image) {
    return image.remoteName == 'core';
  }

  bool isUbuntuRelease(ImageInfo image) {
    return image.remoteName == 'release';
  }

  bool isUbuntuDaily(ImageInfo image) {
    return image.remoteName == 'daily';
  }

  bool isOther(ImageInfo image) {
    return image.remoteName == '';
  }

  final releaseImages = images.where((i) => isUbuntuRelease(i));

  final unlistedDailyImages = images.where((i) =>
      isUbuntuDaily(i) && !releaseImages.any((r) => r.release == i.release));

  final ubuntuImages = releaseImages
      .followedBy(unlistedDailyImages)
      .sorted((a, b) => b.release.compareTo(a.release));

  final coreImages = images
      .where((i) => isCore(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  final otherImages = images
      .where((i) => isOther(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  return [
    if (ubuntuImages.isNotEmpty)
      ImageCard(
        imageKey: 'ubuntu-${ubuntuImages.first.release}',
        parentImage: ubuntuImages.firstWhere(
          (i) => i.aliases.any((a) => a == 'lts'),
          orElse: () => ubuntuImages.first,
        ),
        versions: ubuntuImages.toList(),
        width: cardWidth,
      ),
    if (coreImages.isNotEmpty)
      ImageCard(
        imageKey: 'core-${coreImages.first.release}',
        parentImage: coreImages.first,
        versions: coreImages.toList(),
        width: cardWidth,
      ),
    ...otherImages.map((image) {
      return ImageCard(
        imageKey: '${image.os}-${image.release}',
        parentImage: image,
        versions: [image],
        width: cardWidth,
      );
    }),
  ];
}

class CatalogueScreen extends ConsumerWidget {
  static const sidebarKey = 'catalogue';

  const CatalogueScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    final content = ref.watch(imagesProvider).when(
          skipLoadingOnRefresh: false,
          data: _buildCatalogue,
          error: (error, _) {
            final errorMessage = error is GrpcError
                ? (error.message ?? error.toString())
                : error.toString();
            return Center(
              child: Column(
                children: [
                  const SizedBox(height: 32),
                  Text(
                    l10n.catalogueLoadError(errorMessage),
                    style: const TextStyle(fontSize: 16),
                  ),
                  const SizedBox(height: 16),
                  TextButton(
                    onPressed: () => ref.invalidate(imagesProvider),
                    child: Text(l10n.catalogueRefresh),
                  ),
                ],
              ),
            );
          },
          loading: () => const Center(child: CircularProgressIndicator()),
        );

    final welcomeText = Container(
      constraints: const BoxConstraints(maxWidth: 500),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(l10n.catalogueWelcomeTitle,
              style: const TextStyle(fontSize: 37)),
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 8),
            child: Text(
              l10n.catalogueWelcomeBody,
              style: const TextStyle(fontSize: 16),
            ),
          ),
        ],
      ),
    );

    return Scaffold(
      endDrawer: const LaunchForm(),
      body: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 140).copyWith(top: 40),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            welcomeText,
            const Divider(),
            Expanded(child: content),
          ],
        ),
      ),
    );
  }

  Widget _buildCatalogue(List<ImageInfo> images) {
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          LayoutBuilder(
            builder: (_, constraints) {
              const minCardWidth = 285;
              const spacing = 32.0;
              final nCards = max(1, constraints.maxWidth ~/ minCardWidth);
              final whiteSpace = spacing * (nCards - 1);
              final cardWidth = (constraints.maxWidth - whiteSpace) / nCards;
              final cards = _groupAndCreateCards(images, cardWidth);

              // Group cards into rows for IntrinsicHeight
              final rows = <Widget>[];
              for (var i = 0; i < cards.length; i += nCards) {
                final rowCards = cards.skip(i).take(nCards).toList();
                rows.add(
                  IntrinsicHeight(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: rowCards
                          .map(
                              (card) => SizedBox(width: cardWidth, child: card))
                          .intersperse(const SizedBox(width: spacing))
                          .toList(),
                    ),
                  ),
                );
              }

              return Column(
                children:
                    rows.intersperse(const SizedBox(height: spacing)).toList(),
              );
            },
          ),
          const SizedBox(height: 32),
        ],
      ),
    );
  }
}
