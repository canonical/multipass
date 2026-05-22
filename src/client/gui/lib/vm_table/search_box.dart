import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../l10n/app_localizations.dart';

class SearchNameNotifier extends Notifier<String> {
  @override
  String build() {
    return '';
  }

  void set(String value) {
    state = value;
  }
}

final searchNameProvider = NotifierProvider<SearchNameNotifier, String>(
  SearchNameNotifier.new,
);

class SearchBox extends ConsumerWidget {
  const SearchBox({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = AppLocalizations.of(context)!;
    return SizedBox(
      width: 220,
      child: TextField(
        decoration: InputDecoration(
          hintText: l10n.searchBoxHint,
          suffixIcon: const Icon(Icons.search),
        ),
        onChanged: (name) => ref.read(searchNameProvider.notifier).set(name),
      ),
    );
  }
}
