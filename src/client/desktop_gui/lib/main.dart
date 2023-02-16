import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'globals.dart';
import 'vms_table.dart';

main() async {
  runApp(ProviderScope(
    child: MaterialApp(
      title: 'Multipass',
      home: Scaffold(
        body: Consumer(
          builder: (_, ref, __) => ref.watch(vmsInfo).when(
              data: (infoReply) => VMsTable(infos: infoReply.info),
              error: (error, stackTrace) {
                print(stackTrace);
                return Text('$error');
              },
              loading: () => const Text('Loading')),
        ),
      ),
    ),
  ));
}
