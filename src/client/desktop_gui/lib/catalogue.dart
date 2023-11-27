import 'package:collection/collection.dart';
import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import 'providers.dart';

final imagesProvider = FutureProvider(
  (ref) => ref
      .watch(grpcClientProvider)
      .find(blueprints: false)
      .then((r) => r.imagesInfo),
);

class CatalogueScreen extends ConsumerWidget {
  static const sidebarKey = 'catalogue';

  const CatalogueScreen({super.key});

  // sorts the images in a more user-friendly way
  // the current LTS > other releases sorted by most recent > current devel > core images > appliances
  List<ImageInfo> _sortImages(List<ImageInfo> images) {
    final ltsIndex = images.indexWhere(
      (i) => i.aliasesInfo.map((a) => a.alias).contains('lts'),
    );
    final lts = ltsIndex != -1 ? images.removeAt(ltsIndex) : null;

    final develIndex = images.indexWhere(
      (i) => i.aliasesInfo.map((a) => a.alias).contains('devel'),
    );
    final devel = develIndex != -1 ? images.removeAt(develIndex) : null;

    bool coreFilter(ImageInfo i) {
      return i.aliasesInfo.any((a) => a.alias.contains('core'));
    }

    bool applianceFilter(ImageInfo i) {
      return i.aliasesInfo.any((a) => a.remoteName.contains('appliance'));
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
      ...normalImages,
      if (devel != null) devel,
      ...coreImages,
      ...applianceImages,
    ];
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final images = ref.watch(imagesProvider).valueOrNull ?? const [];
    final sortedImages = _sortImages([...images]);
    final imageList = SingleChildScrollView(
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
          LayoutBuilder(builder: (_, constraints) {
            const minCardWidth = 285;
            const spacing = 32.0;
            final nCards = constraints.maxWidth ~/ minCardWidth;
            final whiteSpace = spacing * (nCards - 1);
            final cardWidth = (constraints.maxWidth - whiteSpace) / nCards;
            return Wrap(
              runSpacing: spacing,
              spacing: spacing,
              children: sortedImages
                  .map((image) => ImageCard(image, cardWidth))
                  .toList(),
            );
          }),
          const SizedBox(height: 32),
        ],
      ),
    );

    final welcomeText = Container(
      constraints: const BoxConstraints(maxWidth: 500),
      child: const Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Welcome to Multipass',
            style: TextStyle(fontSize: 37),
          ),
          Padding(
            padding: EdgeInsets.symmetric(vertical: 8),
            child: Text(
              'Get an instant VM or Appliance in seconds. Multipass can launch and run virtual machines and configure them like a public cloud.',
              style: TextStyle(fontSize: 16),
            ),
          ),
        ],
      ),
    );

    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 140).copyWith(top: 40),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            welcomeText,
            const Divider(),
            Expanded(child: imageList),
          ],
        ),
      ),
    );
  }
}

class ImageCard extends StatelessWidget {
  final ImageInfo image;
  final double width;

  const ImageCard(this.image, this.width, {super.key});

  @override
  Widget build(BuildContext context) {
    return AnimatedContainer(
      duration: const Duration(milliseconds: 100),
      height: 275,
      width: width,
      decoration: BoxDecoration(
        border: Border.all(color: const Color(0xffdddddd)),
        borderRadius: BorderRadius.circular(2),
      ),
      padding: const EdgeInsets.all(16),
      child: DefaultTextStyle(
        style: const TextStyle(fontSize: 16, color: Colors.black),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            SvgPicture.asset('assets/ubuntu.svg'),
            Padding(
              padding: const EdgeInsets.only(bottom: 4, top: 18),
              child: Text(
                '${image.os} ${image.release}',
                style: const TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            Row(
              crossAxisAlignment: CrossAxisAlignment.center,
              children: [
                const Text(
                  'Canonical ',
                  style: TextStyle(color: Color(0xff666666)),
                ),
                SvgPicture.asset('assets/verified.svg')
              ],
            ),
            Expanded(
              child: Align(
                alignment: const Alignment(-1, 0.2),
                child: Text(image.codename),
              ),
            ),
            Row(
              children: [
                OutlinedButton(
                  onPressed: () {},
                  style: OutlinedButton.styleFrom(
                    fixedSize: const Size(83, 36),
                    foregroundColor: Colors.black,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(2),
                    ),
                    side: const BorderSide(color: Color(0xff757575)),
                  ),
                  child: const Text('Launch'),
                ),
                const SizedBox(width: 16),
                OutlinedButton(
                  onPressed: () {},
                  style: OutlinedButton.styleFrom(
                    fixedSize: const Size(48, 36),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(2),
                    ),
                    side: const BorderSide(color: Color(0xff757575)),
                  ),
                  child: SvgPicture.asset('assets/settings.svg',
                      colorFilter: const ColorFilter.mode(
                        Colors.black,
                        BlendMode.srcIn,
                      )),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
