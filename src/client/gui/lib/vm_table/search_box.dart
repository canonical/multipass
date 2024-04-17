import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final searchNameProvider = StateProvider((_) => '');

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
        onChanged: (name) => ref.read(searchNameProvider.notifier).state = name,
      ),
    );
  }
}
