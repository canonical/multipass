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
        cursorColor: Colors.black,
        decoration: const InputDecoration(
          hintText: 'Search instances...',
          suffixIcon: Icon(Icons.search, color: Colors.grey),
          isDense: true,
          border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          focusedBorder: OutlineInputBorder(
            borderRadius: BorderRadius.zero,
            borderSide: BorderSide(),
          ),
        ),
        textAlignVertical: TextAlignVertical.center,
        onChanged: (name) => ref.read(searchNameProvider.notifier).state = name,
      ),
    );
  }
}
