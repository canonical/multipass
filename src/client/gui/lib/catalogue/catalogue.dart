import 'dart:async';
import 'dart:math';

import 'package:basics/basics.dart';
import 'package:collection/collection.dart';
import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:grpc/grpc.dart';

import '../generated/multipass.pb.dart';
import '../providers.dart';
import 'image_card.dart';
import 'launch_form.dart';

// Information about non-Ubuntu distributions
final placeholderImages = [
  (
    image: FindReply_ImageInfo(
      os: 'Fedora',
      release: '39',
      version: '20240401',
      aliasesInfo: [FindReply_AliasInfo(alias: 'fedora', remoteName: 'fedora')],
    ),
    description: 'Community Linux distribution with rolling updates.'
  ),
  (
    image: FindReply_ImageInfo(
      os: 'Debian',
      release: '12',
      version: '20240401',
      aliasesInfo: [FindReply_AliasInfo(alias: 'debian', remoteName: 'debian')],
    ),
    description: 'General purpose Linux distribution.'
  ),
  (
    image: FindReply_ImageInfo(
      os: 'Arch',
      release: '2024.04.01',
      version: '20240401',
      aliasesInfo: [FindReply_AliasInfo(alias: 'arch', remoteName: 'arch')],
    ),
    description: 'Rolling release Linux distribution.'
  ),
  (
    image: FindReply_ImageInfo(
      os: 'RHEL',
      release: '9',
      version: '20240401',
      aliasesInfo: [FindReply_AliasInfo(alias: 'rhel', remoteName: 'rhel')],
    ),
    description: 'Red Hat Enterprise Linux'
  ),
  (
    image: FindReply_ImageInfo(
      os: 'SLES',
      release: '15',
      version: '20240401',
      aliasesInfo: [FindReply_AliasInfo(alias: 'sles', remoteName: 'sles')],
    ),
    description: 'SUSE Linux Enterprise Server'
  ),
  (
    image: FindReply_ImageInfo(
      os: 'NixOS',
      release: '24.05',
      version: '20240401',
      aliasesInfo: [FindReply_AliasInfo(alias: 'nixos', remoteName: 'nixos')],
    ),
    description:
        'Declarative Linux distribution with reproducible system configuration.'
  ),
];

final imagesProvider = FutureProvider<List<ImageInfo>>((ref) async {
  if (ref.watch(daemonAvailableProvider)) {
    final images = ref
        .watch(grpcClientProvider)
        .find(blueprints: false)
        .then((r) => sortImages(r.imagesInfo));
    // artificial delay so that we can see the loading spinner a bit
    // otherwise the reply arrives too quickly and we only see a flash of the spinner
    await Future.delayed(1.seconds);
    return await images;
  }

  return ref.state.valueOrNull ?? await Completer<List<ImageInfo>>().future;
});

// sorts the images in a more user-friendly way
// the current LTS > other releases sorted by most recent > current devel > core images > appliances
List<ImageInfo> sortImages(List<ImageInfo> images) {
  final ltsIndex = images.indexWhere((image) {
    return image.aliasesInfo.any((a) => a.alias == 'lts');
  });
  final lts = ltsIndex != -1 ? images.removeAt(ltsIndex) : null;

  final develIndex = images.indexWhere((image) {
    return image.aliasesInfo.any((a) => a.alias == 'devel');
  });
  final devel = develIndex != -1 ? images.removeAt(develIndex) : null;

  bool coreFilter(ImageInfo image) {
    return image.aliasesInfo.any((a) => a.alias.contains('core'));
  }

  bool applianceFilter(ImageInfo image) {
    return image.aliasesInfo.any((a) => a.remoteName.contains('appliance'));
  }

  int decreasingReleaseSorter(ImageInfo a, ImageInfo b) {
    return b.release.compareTo(a.release);
  }

  final normalImages = images
      .whereNot(coreFilter)
      .whereNot(applianceFilter)
      .sorted(decreasingReleaseSorter);
  final coreImages = images.where(coreFilter).sorted(decreasingReleaseSorter);
  final applianceImages = images.where(applianceFilter);

  return [
    if (lts != null) lts,
    ...normalImages, // All regular Ubuntu releases
    if (devel != null) devel,
    ...coreImages, // All Ubuntu Core releases
    ...applianceImages,
    placeholderImages[3].image, // RHEL
    placeholderImages[4].image, // SLES
    placeholderImages[1].image, // Debian
    placeholderImages[2].image, // Arch
    placeholderImages[5].image, // NixOS
    placeholderImages[0].image, // Fedora
  ];
}

List<Widget> _groupAndCreateCards(List<ImageInfo> images, double cardWidth) {
  bool isCore(ImageInfo image) {
    return image.aliasesInfo.any((a) => a.alias.contains('core'));
  }

  bool isUbuntu(ImageInfo image) {
    return image.os.toLowerCase() == 'ubuntu';
  }

  bool isAppliance(ImageInfo image) {
    return image.aliasesInfo.any((a) => a.remoteName.contains('appliance'));
  }

  bool isDevel(ImageInfo image) {
    return image.aliasesInfo.any((a) => a.alias == 'devel');
  }

  // Group all Ubuntu versions (including devel, excluding Core and appliances)
  final ubuntuImages = images
      .where((i) => isUbuntu(i) && !isCore(i) && !isAppliance(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  // Group Ubuntu Core images
  final ubuntuCoreImages = images
      .where((i) => isUbuntu(i) && isCore(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  // Group Appliances
  final applianceImages = images
      .where((i) => isAppliance(i))
      .sorted((a, b) => b.release.compareTo(a.release));

  // Other distros (non-Ubuntu)
  final otherImages =
      images.where((i) => !isUbuntu(i) && !isAppliance(i)).toList();

  return [
    if (ubuntuImages.isNotEmpty)
      ImageCard(
        mainImage: ubuntuImages.first,
        versions: ubuntuImages.toList(),
        width: cardWidth,
        description:
            'Industry-leading Linux platform for server computing and cloud.',
      ),
    if (ubuntuCoreImages.isNotEmpty)
      ImageCard(
        mainImage: ubuntuCoreImages.first,
        versions: ubuntuCoreImages.toList(),
        width: cardWidth,
        description:
            'Secure and reliable Ubuntu optimized for IoT and embedded devices.',
      ),
    if (applianceImages.isNotEmpty)
      ImageCard(
        mainImage: applianceImages.first,
        versions: applianceImages.toList(),
        width: cardWidth,
        description:
            'Pre-configured Ubuntu images optimized for specific workloads.',
      ),
    ...otherImages.map((image) {
      final description = placeholderImages
          .firstWhere((p) => p.image.os == image.os)
          .description;
      return ImageCard(
        mainImage: image,
        versions: [image],
        width: cardWidth,
        description: description,
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
              child: Column(children: [
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
              ]),
            );
          },
          loading: () => const Center(child: CircularProgressIndicator()),
        );

    final welcomeText = Container(
      constraints: const BoxConstraints(maxWidth: 500),
      child: const Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Welcome to Multipass',
              style: TextStyle(fontSize: 37, fontWeight: FontWeight.w300)),
          Padding(
            padding: EdgeInsets.symmetric(vertical: 8),
            child: Text(
              'Get an instant VM or Appliance in seconds. Multipass can launch and run virtual machines and configure them like a public cloud.',
              style: TextStyle(fontSize: 16, fontWeight: FontWeight.w300),
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
            const SizedBox(height: 32),
            Expanded(child: content),
          ],
        ),
      ),
    );
  }

  Widget _buildCatalogue(List<ImageInfo> images) {
    return SingleChildScrollView(
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        LayoutBuilder(builder: (_, constraints) {
          const minCardWidth = 285;
          const maxColumns = 3;
          const spacing = 16.0;
          final nCards = min(maxColumns, constraints.maxWidth ~/ minCardWidth);
          final whiteSpace = spacing * (nCards - 1);
          final cardWidth = (constraints.maxWidth - whiteSpace) / nCards;
          return Wrap(
            runSpacing: spacing,
            spacing: spacing,
            children: _groupAndCreateCards(images, cardWidth),
          );
        }),
        const SizedBox(height: 32),
      ]),
    );
  }
}
