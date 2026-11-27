// ignore: deprecated_member_use
import 'package:flutter/material.dart' as legacy;
import 'package:flutter_localizations/flutter_localizations.dart'
    as legacy_l10n;
import 'package:material_ui/material_ui.dart';

/// SDK delegates for legacy Material widgets and their root-overlay menus.
const legacyLocalizationsDelegates = <LocalizationsDelegate<dynamic>>[
  // ignore: deprecated_member_use
  legacy_l10n.GlobalMaterialLocalizations.delegate,
  legacy_l10n.GlobalWidgetsLocalizations.delegate,
  _LegacyMaterialFallbackDelegate(),
];

/// Provides the theme and localizations for legacy Material widgets.
class LegacyMaterialBridge extends StatelessWidget {
  const LegacyMaterialBridge({required this.child, super.key});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    // ignore: deprecated_member_use
    return MaterialUiCompatibilityBridge(
      delegates: legacyLocalizationsDelegates,
      child: child,
    );
  }
}

// Use English Material labels for locales unsupported by Flutter.
class _LegacyMaterialFallbackDelegate
    extends LocalizationsDelegate<legacy.MaterialLocalizations> {
  const _LegacyMaterialFallbackDelegate();

  @override
  bool isSupported(Locale locale) => true;

  @override
  Future<legacy.MaterialLocalizations> load(Locale locale) =>
      legacy.DefaultMaterialLocalizations.load(locale);

  @override
  bool shouldReload(_LegacyMaterialFallbackDelegate old) => false;
}
