import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

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
    return SizedBox(
      width: 220,
      child: TextField(
        decoration: const InputDecoration(
          hintText: 'Search instances...',
          suffixIcon: Icon(Icons.search),
        ),
        onChanged: (name) => ref.read(searchNameProvider.notifier).set(name),
      ),
    );
  }
}
