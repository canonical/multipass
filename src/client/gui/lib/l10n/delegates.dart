import 'package:material_ui/material_ui.dart';

import 'app_localizations.dart';
import 'legacy_material_bridge.dart';

/// Localization delegates for the application and standalone Material UI.
final appLocalizationsDelegates = <LocalizationsDelegate<dynamic>>[
  AppLocalizations.delegate,
  ...GlobalMaterialLocalizations.delegates,
  ...legacyLocalizationsDelegates,
];
