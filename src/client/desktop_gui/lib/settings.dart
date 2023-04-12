import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

final settingsProvider = Provider<SharedPreferences>(
  (ref) => throw 'Settings not initialized',
);
