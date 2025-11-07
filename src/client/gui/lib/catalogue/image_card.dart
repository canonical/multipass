import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../providers.dart';
import 'catalogue.dart';
import 'launch_form.dart';

class ImageCard extends ConsumerWidget {
  final ImageInfo parentImage;
  final List<ImageInfo> versions;
  final double width;
  final String imageKey;

  const ImageCard({
    required this.parentImage,
    required this.versions,
    required this.width,
    required this.imageKey,
  });

  String _getParentImageLogo(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu' => 'assets/ubuntu.svg',
      'debian' => 'assets/debian.svg',
      'fedora' => 'assets/fedora.svg',
      _ => 'assets/ubuntu.svg',
    };
  }

  String _getDisplayTitle(ImageInfo parentImage) {
    return switch (parentImage.os.toLowerCase()) {
      'ubuntu' when parentImage.aliases.any((a) => a.contains('core')) =>
        'Ubuntu Core',
      'ubuntu' => 'Ubuntu Server',
      'debian' => 'Debian',
      'fedora' => 'Fedora',
      _ => parentImage.os, // Default case: return the OS name as-is
    };
  }

  String _getDescription(ImageInfo parentImage) {
    return switch (parentImage.os.toLowerCase()) {
      'ubuntu' when parentImage.aliases.any((a) => a.contains('core')) =>
        'Ubuntu operating system optimised for IoT and Edge',
      'ubuntu' =>
        'Ubuntu operating system designed as a backbone for the internet',
      'debian' => 'Debian official cloud image',
      'fedora' => 'Fedora Cloud Edition',
      _ => '',
    };
  }

  bool _shouldShowVersionDropdown() {
    return versions.length > 1;
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final selectedImage =
        ref.watch(selectedImageProvider(imageKey)) ?? parentImage;

    if (ref.read(selectedImageProvider(imageKey)) == null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        ref.read(selectedImageProvider(imageKey).notifier).set(parentImage);
      });
    }

    return Container(
      width: width,
      decoration: BoxDecoration(
        border: Border.all(color: const Color(0xff707070)),
      ),
      padding: const EdgeInsets.all(16),
      child: DefaultTextStyle(
        style: const TextStyle(
          color: Colors.black,
          fontFamily: 'Ubuntu',
          fontSize: 16,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              crossAxisAlignment: CrossAxisAlignment.center,
              children: [
                SvgPicture.asset(
                  _getParentImageLogo(parentImage.os),
                  height: 24,
                  fit: BoxFit.contain,
                  semanticsLabel: '${parentImage.os} logo',
                ),
                const SizedBox(width: 8),
                Text(
                  _getDisplayTitle(parentImage),
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 24,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Text(_getDescription(parentImage),
                style: const TextStyle(fontWeight: FontWeight.w300)),
            const SizedBox(height: 16),
            const Spacer(),
            if (_shouldShowVersionDropdown()) ...[
              Container(
                width: double.infinity,
                decoration: BoxDecoration(
                  color: const Color(0xfff5f5f5),
                ),
                padding: const EdgeInsets.symmetric(horizontal: 8),
                child: DropdownButton<String>(
                  value: selectedImage.release,
                  icon: const Icon(Icons.keyboard_arrow_down),
                  focusColor: Colors.transparent,
                  isExpanded: true,
                  items: versions
                      .map((v) => DropdownMenuItem(
                            value: v.release,
                            child: Text('${v.release} (${v.codename})'),
                          ))
                      .toList(),
                  onChanged: (String? newValue) {
                    if (newValue != null) {
                      final newImage = versions.firstWhere(
                        (v) => v.release == newValue,
                      );
                      ref
                          .read(selectedImageProvider(imageKey).notifier)
                          .set(newImage);
                    }
                  },
                ),
              ),
              const SizedBox(height: 8),
            ] else
              const SizedBox(height: 8),
            const SizedBox(height: 16),
            Row(children: [
              TextButton(
                onPressed: () {
                  final name = ref.read(randomNameProvider);
                  final alias = selectedImage.aliases.first;
                  final launchRequest = LaunchRequest(
                    instanceName: name,
                    image: alias,
                    numCores: defaultCpus,
                    memSize: '${defaultRam}B',
                    diskSpace: '${defaultDisk}B',
                    remoteName: selectedImage.hasRemoteName()
                        ? selectedImage.remoteName
                        : null,
                  );

                  initiateLaunchFlow(ref, launchRequest);
                },
                child: const Text('Launch'),
              ),
              const SizedBox(width: 8),
              OutlinedButton(
                onPressed: () {
                  ref.read(launchingImageProvider.notifier).state =
                      selectedImage;
                  Scaffold.of(context).openEndDrawer();
                },
                child: const Text('Configure'),
              ),
            ]),
          ],
        ),
      ),
    );
  }
}
