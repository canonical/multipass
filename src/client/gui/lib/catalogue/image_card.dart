import 'package:flutter/material.dart' hide ImageInfo;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../providers.dart';
import 'launch_form.dart';

class ImageCard extends ConsumerStatefulWidget {
  final ImageInfo mainImage;
  final List<ImageInfo> versions;
  final double width;
  final String description;

  const ImageCard({
    required this.mainImage,
    required this.versions,
    required this.width,
    required this.description,
    super.key,
  });

  @override
  ConsumerState<ImageCard> createState() => _ImageCardState();
}

class _ImageCardState extends ConsumerState<ImageCard> {
  late ImageInfo selectedImage;

  @override
  void initState() {
    super.initState();
    selectedImage = widget.mainImage;
  }

  String _getDistroLogo(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu' => 'assets/ubuntu.svg',
      'fedora' => 'assets/fedora.svg',
      'debian' => 'assets/debian.svg',
      'arch' => 'assets/arch.svg',
      'rhel' => 'assets/redhat.svg',
      'sles' => 'assets/suse.svg',
      'nixos' => 'assets/nixos.svg',
      _ => 'assets/ubuntu.svg', // fallback to ubuntu
    };
  }

  String _getDistributor(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu' => 'Canonical',
      'fedora' => 'Fedora Project',
      'debian' => 'Debian',
      'arch' => 'Arch Linux',
      'rhel' => 'Red Hat',
      _ => 'Unknown',
    };
  }

  String _getDisplayTitle(String os) {
    return switch (os.toLowerCase()) {
      'ubuntu'
          when widget.mainImage.aliasesInfo
              .any((a) => a.hasRemoteName() && a.remoteName == 'appliance') =>
        'Core Appliances',
      'ubuntu'
          when widget.mainImage.aliasesInfo
              .any((a) => a.alias.contains('core')) =>
        'Ubuntu Core',
      'ubuntu' => 'Ubuntu Server',
      'arch' => 'Arch Linux',
      'rhel' => 'RHEL',
      'sles' => 'SLES',
      'nixos' => 'NixOS',
      'fedora' => 'Fedora',
      'debian' => 'Debian',
      String s => s.toUpperCase(), // Fallback: uppercase for unknown OS names
    };
  }

  bool _shouldShowVersionDropdown(String os) {
    return os.toLowerCase() == 'ubuntu';
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 300,
      width: widget.width.clamp(285, 450),
      decoration: BoxDecoration(
        border: Border.all(color: const Color(0xffdddddd)),
        borderRadius: BorderRadius.circular(2),
      ),
      padding: const EdgeInsets.all(12),
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
                  _getDistroLogo(widget.mainImage.os),
                  height: 24,
                  fit: BoxFit.contain,
                  placeholderBuilder: (BuildContext context) => Container(
                    height: 24,
                    child: const CircularProgressIndicator(),
                  ),
                  semanticsLabel: '${widget.mainImage.os} logo',
                ),
                const SizedBox(width: 8),
                Text(
                  _getDisplayTitle(widget.mainImage.os),
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 24,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(widget.description,
                      style: const TextStyle(fontWeight: FontWeight.w300)),
                  if (_shouldShowVersionDropdown(widget.mainImage.os)) ...[
                    const SizedBox(height: 8),
                    SizedBox(
                      width: double.infinity,
                      child: DropdownButton<String>(
                        value: selectedImage.release,
                        icon: const Icon(Icons.arrow_drop_down),
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
                  ],
                  const Spacer(),
                ],
              ),
            ),
            const SizedBox(height: 8),
            Row(children: [
              TextButton(
                onPressed: () {
                  final name = ref.read(randomNameProvider);
                  final aliasInfo = selectedImage.aliasesInfo.first;
                  final launchRequest = LaunchRequest(
                    instanceName: name,
                    image: aliasInfo.alias,
                    numCores: defaultCpus,
                    memSize: '${defaultRam}B',
                    diskSpace: '${defaultDisk}B',
                    remoteName:
                        aliasInfo.hasRemoteName() ? aliasInfo.remoteName : null,
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
