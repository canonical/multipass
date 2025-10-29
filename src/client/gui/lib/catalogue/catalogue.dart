import 'dart:async';

import 'package:basics/basics.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';

import '../providers.dart';
import 'image_card.dart';
import 'launch_form.dart';

final imagesProvider = FutureProvider<List<ImageInfo>>((ref) async {
  if (!ref.watch(daemonAvailableProvider)) {
    // When daemon is not available, return empty list
    // The UI can handle this state appropriately
    return [];
  }

  final images = await ref
      .watch(grpcClientProvider)
      .find()
      .then((r) => sortImages(r.imagesInfo));

  // artificial delay so that we can see the loading spinner a bit
  // otherwise the reply arrives too quickly and we only see a flash of the spinner
  await Future.delayed(1.seconds);

  return images;
});

// sorts the images in a more user-friendly way
// the current LTS > other releases sorted by most recent > current devel > core images
List<ImageInfo> sortImages(List<ImageInfo> images) {
  final ltsIndex = images.indexWhere((image) {
    return image.aliases.any((a) => a == 'lts');
  });
  final lts = ltsIndex != -1 ? images.removeAt(ltsIndex) : null;

  final develIndex = images.indexWhere((image) {
    return image.aliases.any((a) => a == 'devel');
  });
  final devel = develIndex != -1 ? images.removeAt(develIndex) : null;

  bool coreFilter(ImageInfo image) {
    return image.aliases.any((a) => a.contains('core'));
  }

  bool ubuntuFilter(ImageInfo image) {
    return image.os.toLowerCase() == 'ubuntu';
  }

  int decreasingReleaseSorter(ImageInfo a, ImageInfo b) {
    return b.release.compareTo(a.release);
  }

  final ubuntuImages = images
      .whereNot(coreFilter)
      .where(ubuntuFilter)
      .sorted(decreasingReleaseSorter);
  final coreImages = images.where(coreFilter).sorted(decreasingReleaseSorter);
  final thirdPartyImages = images
      .whereNot(coreFilter)
      .whereNot(ubuntuFilter)
      .sorted(decreasingReleaseSorter);

  return [
    if (lts != null) lts,
    ...ubuntuImages,
    if (devel != null) devel,
    ...coreImages,
    ...thirdPartyImages,
  ];
}

List<Widget> _groupAndCreateCards(List<ImageInfo> images, double cardWidth) {
  bool isCore(ImageInfo image) {
    return image.aliases.any((a) => a.contains('core'));
  }

  bool isUbuntu(ImageInfo image) {
    return image.os.toLowerCase() == 'ubuntu';
  }

  bool isOther(ImageInfo image) {
    return !isCore(image) && !isUbuntu(image);
  }

  final ubuntuImages = images
      .where((i) => isUbuntu(i) && !isCore(i) && !isOther(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  final coreImages = images
      .where((i) => isUbuntu(i) && isCore(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  final otherImages = images
      .where((i) => isOther(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  return [
    if (ubuntuImages.isNotEmpty)
      ImageCard(
        parentImage: ubuntuImages.first,
        versions: ubuntuImages.toList(),
        width: cardWidth,
      ),
    if (coreImages.isNotEmpty)
      ImageCard(
        parentImage: coreImages.first,
        versions: coreImages.toList(),
        width: cardWidth,
      ),
    ...otherImages.map((image) {
      return ImageCard(
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
    final content = ref.watch(imagesProvider).when(
          skipLoadingOnRefresh: false,
          data: _buildCatalogue,
          error: (error, _) {
            final errorMessage = error is GrpcError ? error.message : error;
            return Center(
              child: Column(
                children: [
                  const SizedBox(height: 32),
                  Text(
                    'Failed to retrieve images: $errorMessage',
                    style: const TextStyle(fontSize: 16),
                  ),
                  const SizedBox(height: 16),
                  TextButton(
                    onPressed: () => ref.invalidate(imagesProvider),
                    child: const Text('Refresh'),
                  ),
                ],
              ),
            );
          },
          loading: () => const Center(child: CircularProgressIndicator()),
        );

    final welcomeText = Container(
      constraints: const BoxConstraints(maxWidth: 500),
      child: const Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Welcome to Multipass', style: TextStyle(fontSize: 37)),
          Padding(
            padding: EdgeInsets.symmetric(vertical: 8),
            child: Text(
              'Get an instant VM in seconds. Multipass can launch and run virtual machines and configure them like a public cloud.',
              style: TextStyle(fontSize: 16),
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
          Container(
            margin: const EdgeInsets.symmetric(vertical: 10),
            child: const Text(
              'Images',
              style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
            ),
          ),
          LayoutBuilder(
            builder: (_, constraints) {
              const minCardWidth = 285;
              const spacing = 32.0;
              final nCards = constraints.maxWidth ~/ minCardWidth;
              final whiteSpace = spacing * (nCards - 1);
              final cardWidth = (constraints.maxWidth - whiteSpace) / nCards;
              return Wrap(
                runSpacing: spacing,
                spacing: spacing,
                children:
                    images.map((image) => ImageCard(image, cardWidth)).toList(),
              );
            },
          ),
          const SizedBox(height: 32),
        ],
      ),
    );
  }
}
