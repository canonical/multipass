// Single entry point that runs all integration tests in one build+process.
import 'bridge_network_test.dart' as bridge_network_test;
import 'bulk_actions_test.dart' as bulk_actions_test;
import 'catalogue_test.dart' as catalogue_test;
import 'daemon_unavailable_test.dart' as daemon_unavailable_test;
import 'empty_state_catalogue_link_test.dart'
    as empty_state_catalogue_link_test;
import 'launch_test.dart' as launch_test;
import 'launch_form_test.dart' as launch_form_test;
import 'vm_details_mounts_test.dart' as vm_details_mounts_test;
import 'vm_lifecycle_test.dart' as vm_lifecycle_test;
import 'vm_list_test.dart' as vm_list_test;

void main() {
  bridge_network_test.main();
  bulk_actions_test.main();
  catalogue_test.main();
  daemon_unavailable_test.main();
  empty_state_catalogue_link_test.main();
  launch_test.main();
  // launch_form_test.main();
  // vm_details_mounts_test.main();
  vm_lifecycle_test.main();
  vm_list_test.main();
}
