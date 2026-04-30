import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../l10n/app_localizations.dart';
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

  String _getDisplayTitle(ImageInfo parentImage, AppLocalizations l10n) {
    return switch (parentImage.os.toLowerCase()) {
      'ubuntu' when parentImage.aliases.any((a) => a.contains('core')) =>
        l10n.imageCardTitleUbuntuCore,
      'ubuntu' => l10n.imageCardTitleUbuntuServer,
      'debian' => l10n.imageCardTitleDebian,
      'fedora' => l10n.imageCardTitleFedora,
      _ => parentImage.os, // Default case: return the OS name as-is
    };
  }

  String _getDescription(ImageInfo parentImage, AppLocalizations l10n) {
    return switch (parentImage.os.toLowerCase()) {
      'ubuntu' when parentImage.aliases.any((a) => a.contains('core')) =>
        l10n.imageCardDescUbuntuCore,
      'ubuntu' => l10n.imageCardDescUbuntuServer,
      'debian' => l10n.imageCardDescDebian,
      'fedora' => l10n.imageCardDescFedora,
      _ => '',
    };
  }

  String _getVersionLabel(ImageInfo version) {
    return version.release.toLowerCase() == version.codename.toLowerCase()
        ? version.release
        : '${version.release} (${version.codename})';
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
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
                  semanticsLabel: l10n.imageCardLogoSemantics(parentImage.os),
                ),
                const SizedBox(width: 8),
                Text(
                  _getDisplayTitle(parentImage, l10n),
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 24,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Text(_getDescription(parentImage, l10n),
                style: const TextStyle(fontWeight: FontWeight.w300)),
            const SizedBox(height: 16),
            const Spacer(),
            Container(
              width: double.infinity,
              decoration: BoxDecoration(
                color: const Color(0xfff5f5f5),
                border: Border(
                  bottom: BorderSide(color: Colors.black, width: 1),
                ),
              ),
              padding: const EdgeInsets.symmetric(horizontal: 8),
              child: DropdownButton<String>(
                value: selectedImage.release,
                icon: const Icon(Icons.keyboard_arrow_down),
                focusColor: Colors.transparent,
                isExpanded: true,
                underline: const SizedBox(),
                items: versions
                    .map((v) => DropdownMenuItem(
                          value: v.release,
                          child: Text(
                            _getVersionLabel(v),
                            overflow: TextOverflow.ellipsis,
                            maxLines: 1,
                          ),
                        ))
                    .toList(),
                onChanged: versions.length > 1
                    ? (String? newValue) {
                        if (newValue != null) {
                          final newImage = versions.firstWhere(
                            (v) => v.release == newValue,
                          );
                          ref
                              .read(selectedImageProvider(imageKey).notifier)
                              .set(newImage);
                        }
                      }
                    : null,
              ),
            ),
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
                child: Text(l10n.vmTableLaunch),
              ),
              const SizedBox(width: 8),
              OutlinedButton(
                onPressed: () {
                  ref.read(launchingImageProvider.notifier).state =
                      selectedImage;
                  Scaffold.of(context).openEndDrawer();
                },
                child: Text(l10n.dialogConfigure),
              ),
            ]),
          ],
        ),
      ),
    );
  }
}
