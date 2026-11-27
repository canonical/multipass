import 'dart:convert';

import 'package:file_selector_platform_interface/file_selector_platform_interface.dart';
import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/catalogue/cloud_init_input.dart';
import 'package:multipass_gui/l10n/app_localizations.dart';
import 'package:multipass_gui/l10n/delegates.dart';
import 'package:multipass_gui/l10n/legacy_material_bridge.dart';
import 'package:multipass_gui/l10n/app_localizations_en.dart';
import 'package:multipass_gui/notifications.dart';
import 'package:re_editor/re_editor.dart';

final _l10n = AppLocalizationsEn();

class _FakeFileSelector extends FileSelectorPlatform {
  _FakeFileSelector(this.pickFile);

  final Future<XFile?> Function() pickFile;

  @override
  Future<XFile?> openFile({
    List<XTypeGroup>? acceptedTypeGroups,
    String? initialDirectory,
    String? confirmButtonText,
  }) {
    return pickFile();
  }
}

Future<ProviderContainer> _pumpInput(
  WidgetTester tester, {
  FormFieldSetter<String>? onSaved,
}) async {
  final container = ProviderContainer();
  addTearDown(container.dispose);
  // notificationsProvider is auto-disposed, so keep it alive to inspect it.
  container.listen(notificationsProvider, (_, __) {});

  await tester.pumpWidget(
    UncontrolledProviderScope(
      container: container,
      child: MaterialApp(
        builder: (context, child) => LegacyMaterialBridge(child: child!),
        localizationsDelegates: appLocalizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: Form(
            autovalidateMode: AutovalidateMode.always,
            child: CloudInitInput(onSaved: onSaved ?? (_) {}),
          ),
        ),
      ),
    ),
  );
  return container;
}

Future<void> _setUserData(WidgetTester tester, String userData) async {
  tester.widget<CodeEditor>(find.byType(CodeEditor)).controller!.text =
      userData;
  await tester.pump();
}

String? _errorText(WidgetTester tester) => tester
    .widget<InputDecorator>(
      find.ancestor(
        of: find.byType(CodeEditor),
        matching: find.byType(InputDecorator),
      ),
    )
    .decoration
    .errorText;

void _save(WidgetTester tester) =>
    tester.state<FormState>(find.byType(Form)).save();

void main() {
  late FileSelectorPlatform originalFileSelector;

  setUp(() {
    originalFileSelector = FileSelectorPlatform.instance;
  });

  tearDown(() {
    FileSelectorPlatform.instance = originalFileSelector;
  });

  group('CloudInitInput', () {
    testWidgets('saves the default user data when left untouched', (
      tester,
    ) async {
      String? savedUserData;
      await _pumpInput(tester, onSaved: (value) => savedUserData = value);

      _save(tester);

      expect(_errorText(tester), isNull);
      expect(savedUserData, initialUserData);
    });

    testWidgets('saves the edited user data', (tester) async {
      String? savedUserData;
      await _pumpInput(tester, onSaved: (value) => savedUserData = value);

      await _setUserData(tester, '#cloud-config\npackages: [curl]\n');
      _save(tester);

      expect(savedUserData, '#cloud-config\npackages: [curl]\n');
    });

    for (final (description, userData, error) in [
      (
        'rejects user data without the #cloud-config header',
        'packages: [curl]',
        'Expected first line to be: #cloud-config',
      ),
      (
        'rejects a header with trailing characters',
        '#cloud-config-archive\n- packages: [curl]',
        'Expected first line to be: #cloud-config',
      ),
      (
        'rejects a document root that is not a mapping',
        '#cloud-config\n- curl',
        'Expected a mapping at the document root.',
      ),
      ('rejects malformed YAML', '#cloud-config\npackages: [curl', isNotNull),
      (
        'accepts Windows line endings',
        '#cloud-config\r\npackages: [curl]\r\n',
        isNull,
      ),
      ('accepts a header with only comments', '#cloud-config\n# empty', isNull),
    ]) {
      testWidgets(description, (tester) async {
        await _pumpInput(tester);

        await _setUserData(tester, userData);

        expect(_errorText(tester), error);
      });
    }

    testWidgets('Reset restores the default user data', (tester) async {
      String? savedUserData;
      await _pumpInput(tester, onSaved: (value) => savedUserData = value);
      await _setUserData(tester, 'invalid');

      await tester.tap(find.text(_l10n.launchFormCloudInitReset));
      await tester.pump();
      _save(tester);

      expect(_errorText(tester), isNull);
      expect(savedUserData, initialUserData);
    });

    testWidgets('Load file replaces the user data with the file contents', (
      tester,
    ) async {
      const contents = '#cloud-config\npackages: [jq]\n';
      FileSelectorPlatform.instance = _FakeFileSelector(
        () async => XFile.fromData(utf8.encode(contents)),
      );
      String? savedUserData;
      await _pumpInput(tester, onSaved: (value) => savedUserData = value);

      await tester.tap(find.text(_l10n.launchFormCloudInitLoadFile));
      await tester.pumpAndSettle();
      _save(tester);

      expect(savedUserData, contents);
    });

    testWidgets('Load file keeps the user data when no file is chosen', (
      tester,
    ) async {
      FileSelectorPlatform.instance = _FakeFileSelector(() async => null);
      String? savedUserData;
      await _pumpInput(tester, onSaved: (value) => savedUserData = value);
      await _setUserData(tester, '#cloud-config\n');

      await tester.tap(find.text(_l10n.launchFormCloudInitLoadFile));
      await tester.pumpAndSettle();
      _save(tester);

      expect(savedUserData, '#cloud-config\n');
    });

    testWidgets('Load file shows an error when the file cannot be read', (
      tester,
    ) async {
      final error = Exception('permission denied');
      FileSelectorPlatform.instance = _FakeFileSelector(
        () => Future.error(error),
      );
      final container = await _pumpInput(tester);

      await tester.tap(find.text(_l10n.launchFormCloudInitLoadFile));
      await tester.pumpAndSettle();

      final notification =
          container.read(notificationsProvider).single as ErrorNotification;
      expect(
        (notification.child as Text).data,
        _l10n.launchFormCloudInitFileError(error.toString()),
      );
    });
  });
}
