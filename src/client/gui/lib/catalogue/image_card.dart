import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../providers.dart';
import 'launch_form.dart';

class ImageCard extends ConsumerStatefulWidget {
  final ImageInfo parentImage;
  final List<ImageInfo> versions;
  final double width;

  const ImageCard(
      {required this.parentImage,
      required this.versions,
      required this.width,
      super.key});

  @override
  ConsumerState<ImageCard> createState() => _ImageCardState();
}

class _ImageCardState extends ConsumerState<ImageCard> {
  late ImageInfo selectedImage;

  @override
  void initState() {
    super.initState();
    selectedImage = widget.parentImage;
  }

  String _getParentImageLogo(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu' => 'assets/ubuntu.svg',
      'debian' => 'assets/debian.svg',
      'fedora' => 'assets/fedora.svg',
      _ => 'assets/ubuntu.svg',
    };
  }

  String _getDisplayTitle(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu' when widget.parentImage.aliases.any((a) => a.contains('core')) =>
        'Ubuntu Core',
      'ubuntu' => 'Ubuntu Server',
      'debian' => 'Debian',
      'fedora' => 'Fedora',
      _ => os, // Default case: return the OS name as-is
    };
  }

  String _getDescription(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu' when widget.parentImage.aliases.any((a) => a.contains('core')) =>
        'Ubuntu operating system optimised for IoT and Edge',
      'ubuntu' =>
        'Ubuntu operating system designed as a backbone for the internet',
      'debian' => 'Debian official cloud image',
      'fedora' => 'Fedora Cloud Edition',
      _ => '',
    };
  }

  bool _shouldShowVersionDropdown() {
    return widget.versions.length > 1;
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      width: widget.width,
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
                  _getParentImageLogo(widget.parentImage.os),
                  height: 24,
                  fit: BoxFit.contain,
                  semanticsLabel: '${widget.parentImage.os} logo',
                ),
                const SizedBox(width: 8),
                Text(
                  _getDisplayTitle(widget.parentImage.os),
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 24,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Text(_getDescription(widget.parentImage.os),
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
                  items: widget.versions
                      .map((v) => DropdownMenuItem(
                            value: v.release,
                            child: Text('${v.release} (${v.codename})'),
                          ))
                      .toList(),
                  onChanged: (String? newValue) {
                    if (newValue != null) {
                      setState(() {
                        selectedImage = widget.versions.firstWhere(
                          (v) => v.release == newValue,
                        );
                      });
                    }
                  },
                ),
              ),
              const SizedBox(height: 8),
            ],
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
