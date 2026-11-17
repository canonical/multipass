import 'package:multipass_gui/providers.dart';

// Mirrors the image info the daemon reports for each remote.
final noble = ImageInfo(
  os: 'Ubuntu',
  release: '24.04 LTS',
  codename: 'Noble Numbat',
  aliases: ['24.04', 'noble', 'lts'],
  remoteName: 'release',
);
final jammy = ImageInfo(
  os: 'Ubuntu',
  release: '22.04 LTS',
  codename: 'Jammy Jellyfish',
  aliases: ['22.04', 'jammy'],
  remoteName: 'release',
);
final questing = ImageInfo(
  os: 'Ubuntu',
  release: '25.10',
  codename: 'Questing Quokka',
  aliases: ['25.10', 'questing'],
  remoteName: 'release',
);
final dailyNoble = ImageInfo(
  os: 'Ubuntu',
  release: '24.04 LTS',
  codename: 'Noble Numbat',
  aliases: ['24.04', 'noble', 'lts'],
  remoteName: 'daily',
);
final devel = ImageInfo(
  os: 'Ubuntu',
  release: '26.04',
  codename: 'Resolute Raccoon',
  aliases: ['26.04', 'resolute', 'devel'],
  remoteName: 'daily',
);
final core24 = ImageInfo(
  os: 'Ubuntu',
  release: 'Core 24',
  codename: 'Core 24',
  aliases: ['core24'],
  remoteName: 'core',
);
final core22 = ImageInfo(
  os: 'Ubuntu',
  release: 'Core 22',
  codename: 'Core 22',
  aliases: ['core22'],
  remoteName: 'core',
);
final snapcraft = ImageInfo(
  os: 'Ubuntu',
  release: 'Snapcraft builder for Core 24',
  codename: 'Snapcraft builder for Core 24',
  aliases: ['core24'],
  remoteName: 'snapcraft',
);
final debian = ImageInfo(
  os: 'Debian',
  release: '13',
  codename: 'Trixie',
  aliases: ['debian', 'trixie'],
);
final fedora = ImageInfo(
  os: 'Fedora',
  release: '44',
  codename: '44',
  aliases: ['fedora'],
);
final alpine = ImageInfo(os: 'Alpine', release: '3.20', aliases: ['alpine']);
