import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Compatibility extensions for Riverpod v3 migration.
/// 
/// This file provides extensions that were removed in Riverpod v3
/// to ease the migration from v2 to v3.
/// 
/// Note: Family notifiers (AutoDisposeFamilyNotifier, etc.) were completely
/// redesigned in v3 and cannot be simply shimmed. Those require actual code
/// migration to use the new v3 pattern.

extension AsyncValueCompat<T> on AsyncValue<T> {
  /// Returns the value if this [AsyncValue] is in a data state,
  /// otherwise returns null.
  /// 
  /// This extension provides backward compatibility with Riverpod v2's
  /// `valueOrNull` getter which was removed in v3.
  T? get valueOrNull {
    return when(
      data: (value) => value,
      loading: () => null,
      error: (_, __) => null,
    );
  }
}