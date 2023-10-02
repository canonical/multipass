import 'package:built_collection/built_collection.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers.dart';

final selectedVmsProvider = StateProvider<BuiltSet<String>>((ref) {
  // if any filter is applied (either name or show running only), the provider
  // will be invalidated and return the empty set again
  ref.watch(runningOnlyProvider);
  ref.watch(searchNameProvider);
  // look for changes in available vms and make sure this set does not contain
  // vm names that are no longer present
  ref.listen(vmNamesProvider, (_, availableNames) {
    ref.controller.update(availableNames.intersection);
  });

  return BuiltSet();
});
