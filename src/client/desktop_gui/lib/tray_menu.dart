import 'dart:io';

import 'package:basics/basics.dart';
import 'package:built_collection/built_collection.dart';
import 'package:collection/collection.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';
import 'package:tray_menu/tray_menu.dart';
import 'package:window_manager/window_manager.dart';

import 'ffi.dart';
import 'providers.dart';

const actionAllowedStatuses = {
  'Start': {Status.STOPPED, Status.SUSPENDED},
  'Stop': {Status.RUNNING},
  'Suspend': {Status.RUNNING},
  'Restart': {Status.RUNNING},
  'Delete': {Status.STOPPED, Status.SUSPENDED, Status.RUNNING},
  'Recover': {Status.DELETED},
  'Purge': {Status.DELETED},
};

final trayMenuDataProvider = Provider((ref) {
  final primaryName = ref.watch(clientSettingsProvider.select(
    (settings) => settings[primaryNameKey]!,
  ));
  final vmStatuses = ref.watch(vmInfosStreamProvider.select(
    (value) => value.whenData((data) => infosToStatusMap(data).build()),
  ));
  return (primaryName, vmStatuses);
});

final daemonVersionProvider = Provider((ref) {
  final couldGetData =
      ref.watch(vmInfosStreamProvider.select((value) => value.asData != null));
  if (couldGetData) {
    final client = ref.watch(grpcClientProvider);
    client
        .version()
        .catchError((_) => 'failed to get version')
        .then((version) => ref.state = version);
  }
  return 'loading...';
});

Future<String> _iconFilePath() async {
  final dataDir = await getApplicationSupportDirectory();
  final iconFile = File('${dataDir.path}/icon.ico');
  final data = await rootBundle.load('assets/icon.ico');
  await iconFile.writeAsBytes(data.buffer.asUint8List());
  return iconFile.path;
}

const _separatorInstancesKey = 'separator-instances';
const _separatorPrimaryKey = 'separator-primary';
const _errorKey = 'error';
const _separatorErrorKey = 'separator-error';
const _separatorAboutKey = 'separator-about';

Future<void> setupTrayMenu(ProviderContainer providerContainer) async {
  await TrayMenu.instance.addLabel(
    'toggle-window',
    label: 'Toggle window',
    callback: (_, __) async => await windowManager.isVisible()
        ? windowManager.hide()
        : windowManager.show(),
  );

  await TrayMenu.instance.addSeparator(_separatorAboutKey);
  final aboutSubmenu = await TrayMenu.instance.addSubmenu(
    'about',
    label: 'About',
  );
  await aboutSubmenu.addLabel(
    'multipass-version',
    label: 'multipass version: $multipassVersion',
    enabled: false,
  );
  final deamonVersionItem = await aboutSubmenu.addLabel(
    'multipassd-version',
    label: 'multipassd version: loading...',
    enabled: false,
  );
  providerContainer.listen(
    daemonVersionProvider,
    (_, version) => deamonVersionItem.setLabel('multipassd version: $version'),
  );
  await aboutSubmenu.addLabel(
    'copyright',
    label: 'Copyright (C) Canonical, Ltd.',
    enabled: false,
  );
  await TrayMenu.instance.addLabel(
    'quit',
    label: 'Quit',
    callback: (_, __) => windowManager.destroy(),
  );

  await TrayMenu.instance.show(await _iconFilePath());
  providerContainer.listen(trayMenuDataProvider, (previous, next) {
    final (previousPrimary, previousVmData) = previous!;
    final (nextPrimary, nextData) = next;
    nextData.when(
      data: (nextVms) {
        _updateTrayMenu(
          providerContainer.read(grpcClientProvider),
          previousPrimary,
          previousVmData.valueOrNull?.toMap() ?? {},
          nextPrimary,
          nextVms.toMap(),
        );
      },
      error: (_, __) => _setTrayMenuError(),
      loading: () {},
    );
  });
}

Future<void> _setTrayMenuError() async {
  final keys = TrayMenu.instance.keys
      .where((key) => key.startsWith('instance-') || key.startsWith('primary-'))
      .toList();
  for (final key in keys) {
    await TrayMenu.instance.remove(key);
  }
  await TrayMenu.instance.remove(_separatorPrimaryKey);
  await TrayMenu.instance.remove(_separatorInstancesKey);

  const errorMessage = 'Failed retrieving instance data';
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

Future<void> _updateTrayMenu(
  final GrpcClient grpcClient,
  final String previousPrimary,
  final Map<String, Status> previousVms,
  final String nextPrimary,
  final Map<String, Status> nextVms,
) async {
  startCallback(String name) => (_, __) => grpcClient.start([name]);
  stopCallback(String name) => (_, __) => grpcClient.stop([name]);
  const primaryStartKey = 'primary-start';
  const primaryStopKey = 'primary-stop';

  await TrayMenu.instance.remove(_errorKey);
  await TrayMenu.instance.remove(_separatorErrorKey);

  if (nextPrimary.isEmpty) {
    await TrayMenu.instance.remove(primaryStartKey);
    await TrayMenu.instance.remove(primaryStopKey);
    await TrayMenu.instance.remove(_separatorPrimaryKey);
  } else {
    final status = nextVms.remove(nextPrimary);
    final startLabel = status == null
        ? 'Start'
        : 'Start "$nextPrimary" (${status.toString().toLowerCase().capitalize()})';
    final startEnabled = actionAllowedStatuses['Start']!.contains(status);
    final stopEnabled = actionAllowedStatuses['Stop']!.contains(status);
    final primaryStart = TrayMenu.instance.get<MenuItemLabel>(primaryStartKey);
    final primaryStop = TrayMenu.instance.get<MenuItemLabel>(primaryStopKey);
    if (primaryStart == null || primaryStop == null) {
      final beforeKey = TrayMenu.instance.keys.contains(_separatorInstancesKey)
          ? _separatorInstancesKey
          : _separatorAboutKey;
      await TrayMenu.instance.addSeparator(
        _separatorPrimaryKey,
        before: beforeKey,
      );
      await TrayMenu.instance.addLabel(
        primaryStartKey,
        label: startLabel,
        enabled: startEnabled,
        before: beforeKey,
        callback: startCallback(nextPrimary),
      );
      await TrayMenu.instance.addLabel(
        primaryStopKey,
        label: 'Stop',
        enabled: stopEnabled,
        before: beforeKey,
        callback: stopCallback(nextPrimary),
      );
    } else {
      await primaryStart.setLabel(startLabel);
      await primaryStart.setEnabled(startEnabled);
      primaryStart.callback = startCallback(nextPrimary);
      await primaryStop.setEnabled(stopEnabled);
      primaryStop.callback = stopCallback(nextPrimary);
    }
  }

  for (final name in previousVms.keys.whereNot(nextVms.containsKey)) {
    await TrayMenu.instance.remove('instance-$name');
  }

  if (nextVms.isEmpty) {
    await TrayMenu.instance.remove(_separatorInstancesKey);
  } else if (!TrayMenu.instance.keys.contains(_separatorInstancesKey)) {
    await TrayMenu.instance.addSeparator(
      _separatorInstancesKey,
      before: _separatorAboutKey,
    );
  }

  for (final MapEntry(key: name, value: status) in nextVms.entries) {
    final key = 'instance-$name';
    final previousStatus = previousVms[name];
    final label = '$name (${status.toString().toLowerCase().capitalize()})';
    final startEnabled = actionAllowedStatuses['Start']!.contains(status);
    final stopEnabled = actionAllowedStatuses['Stop']!.contains(status);
    if (previousStatus == null || name == previousPrimary) {
      final submenu = await TrayMenu.instance.addSubmenu(
        key,
        label: label,
        before: _separatorAboutKey,
      );
      await submenu.addLabel(
        'start',
        label: 'Start',
        enabled: startEnabled,
        callback: startCallback(name),
      );
      await submenu.addLabel(
        'stop',
        label: 'Stop',
        enabled: stopEnabled,
        callback: stopCallback(name),
      );
    } else {
      final submenu = TrayMenu.instance.get<MenuItemSubmenu>(key);
      await submenu?.setLabel(label);
      await submenu?.get<MenuItemLabel>('start')?.setEnabled(startEnabled);
      await submenu?.get<MenuItemLabel>('stop')?.setEnabled(stopEnabled);
    }
  }
}
