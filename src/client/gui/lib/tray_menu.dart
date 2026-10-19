import 'dart:async';
import 'dart:io';
import 'dart:ui';

import 'package:basics/basics.dart';
import 'package:collection/collection.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:multipass_gui/vm_details/terminal.dart';
import 'package:path_provider/path_provider.dart';
import 'package:synchronized/synchronized.dart';
import 'package:tray_menu/tray_menu.dart';
import 'package:window_manager/window_manager.dart';

import 'ffi.dart';
import 'l10n/app_localizations.dart';
import 'platform/platform.dart';
import 'providers.dart';
import 'sidebar.dart';
import 'vm_action.dart';
import 'vm_details/terminal_tabs.dart';
import 'vm_details/vm_details.dart';

extension WindowManagerExtensions on WindowManager {
  Future<void> showAndRestore() async {
    await show();
    await restore();
  }
}

AppLocalizations _l10n() {
  try {
    return lookupAppLocalizations(PlatformDispatcher.instance.locale);
  } on FlutterError catch (_) {
    return lookupAppLocalizations(const Locale('en'));
  }
}

Future<String> _iconFilePath() async {
  final dataDir = await getApplicationSupportDirectory();
  final iconName = mpPlatform.trayIconFile;
  final iconFile = File('${dataDir.path}/$iconName');
  final data = await rootBundle.load('assets/$iconName');
  await iconFile.writeAsBytes(data.buffer.asUint8List());
  return iconFile.path;
}

const _separatorVmsKey = 'separator-vms';
const _errorKey = 'error';
const _separatorErrorKey = 'separator-error';
const _separatorAboutKey = 'separator-about';

Future<void> setupTrayMenu(ProviderContainer providerContainer) async {
  if (mpPlatform.showToggleWindow) {
    await TrayMenu.instance.addLabel(
      'toggle-window',
      label: _l10n().trayToggleWindow,
      callback: (_, __) async => await windowManager.isVisible()
          ? windowManager.hide()
          : windowManager.showAndRestore(),
    );
  }

  await TrayMenu.instance.addSeparator(_separatorAboutKey);
  final aboutSubmenu = await TrayMenu.instance.addSubmenu(
    'about',
    label: _l10n().aboutTitle,
  );
  await aboutSubmenu.addLabel(
    'multipass-version',
    label: _l10n().trayMultipassVersion(multipassVersion),
    enabled: false,
  );
  await aboutSubmenu.addLabel(
    'copyright',
    label: _l10n().trayCopyright,
    enabled: false,
  );
  providerContainer.listen<String>(
    daemonVersionProvider,
    (previous, next) async {
      await aboutSubmenu.remove('multipassd-version');
      if (next == multipassVersion) return;
      await aboutSubmenu.addLabel(
        'multipassd-version',
        label: _l10n().trayMultipassdVersion(next),
        enabled: false,
        before: 'copyright',
      );
    },
    fireImmediately: true,
  );
  await TrayMenu.instance.addLabel(
    'quit',
    label: _l10n().trayQuit,
    callback: (_, __) => windowManager.close(),
  );

  await TrayMenu.instance.show(await _iconFilePath());

  final lock = Lock();
  providerContainer.listen(trayMenuDataProvider, (
    previousVmData,
    nextVmData,
  ) async {
    lock.synchronized(() async {
      if (nextVmData == null) {
        await _setTrayMenuError();
      } else {
        await _updateTrayMenu(
          providerContainer,
          previousVmData?.toMap() ?? {},
          nextVmData.toMap(),
        );
      }
    });
  });
}

Future<void> _setTrayMenuError() async {
  final keys =
      TrayMenu.instance.keys.where((key) => key.startsWith('vm-')).toList();
  for (final key in keys) {
    await TrayMenu.instance.remove(key);
  }
  await TrayMenu.instance.remove(_separatorVmsKey);

  final errorMessage = _l10n().trayErrorInstanceData;
  final errorLabel = TrayMenu.instance.get<MenuItemLabel>(_errorKey);
  if (errorLabel != null) {
    await errorLabel.setLabel(errorMessage);
  } else {
    await TrayMenu.instance.addSeparator(
      _separatorErrorKey,
      before: _separatorAboutKey,
    );
    await TrayMenu.instance.addLabel(
      _errorKey,
      label: errorMessage,
      before: _separatorAboutKey,
    );
  }
}

extension on InstanceStatus_Status {
  String get label => toString().toLowerCase().capitalize();
}

Future<void> _updateTrayMenu(
  final ProviderContainer providerContainer,
  final Map<String, Status> previousVms,
  final Map<String, Status> nextVms,
) async {
  final grpcClient = providerContainer.read(grpcClientProvider);

  await TrayMenu.instance.remove(_errorKey);
  await TrayMenu.instance.remove(_separatorErrorKey);

  for (final name in previousVms.keys.whereNot(nextVms.containsKey)) {
    await TrayMenu.instance.remove('vm-$name');
  }

  if (nextVms.isEmpty) {
    await TrayMenu.instance.remove(_separatorVmsKey);
  } else if (!TrayMenu.instance.keys.contains(_separatorVmsKey)) {
    await TrayMenu.instance.addSeparator(
      _separatorVmsKey,
      before: _separatorAboutKey,
    );
  }

  for (final MapEntry(key: name, value: status) in nextVms.entries) {
    final key = 'vm-$name';
    final previousStatus = previousVms[name];
    final label = '$name (${status.label})';
    final startEnabled = VmAction.start.allowedStatuses.contains(status);
    final stopEnabled = VmAction.stop.allowedStatuses.contains(status);
    if (previousStatus == null) {
      final submenu = await TrayMenu.instance.addSubmenu(
        key,
        label: label,
        before: _separatorAboutKey,
      );
      await submenu.addLabel(
        'start',
        label: _l10n().vmActionLabel('start'),
        enabled: startEnabled,
        callback: (_, __) => grpcClient.start([name]),
      );
      await submenu.addLabel(
        'stop',
        label: _l10n().vmActionLabel('stop'),
        enabled: stopEnabled,
        callback: (_, __) => grpcClient.stop([name]),
      );
      await submenu.addSeparator('separator');
      await submenu.addLabel(
        'open',
        label: _l10n().trayOpenInMultipass,
        callback: (_, __) {
          providerContainer
              .read(vmScreenLocationProvider(name).notifier)
              .set(VmDetailsLocation.shells);
          providerContainer.read(sidebarKeyProvider.notifier).set(key);
          final (:ids, :currentIndex) = providerContainer.read(
            shellIdsProvider(name),
          );
          final terminalIdentifier = (vmName: name, shellId: ids[currentIndex]);
          final provider = terminalProvider(terminalIdentifier);
          if (providerContainer.exists(provider)) {
            providerContainer.read(provider.notifier).start();
          }
          windowManager.showAndRestore();
        },
      );
    } else {
      final submenu = TrayMenu.instance.get<MenuItemSubmenu>(key);
      await submenu?.setLabel(label);
      await submenu?.get<MenuItemLabel>('start')?.setEnabled(startEnabled);
      await submenu?.get<MenuItemLabel>('stop')?.setEnabled(stopEnabled);
    }
  }
}
