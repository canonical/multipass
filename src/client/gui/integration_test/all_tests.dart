// Single entry point that runs all integration tests in one build+process.
import 'bulk_actions_test.dart' as bulk_actions_test;
import 'catalogue_test.dart' as catalogue_test;
import 'empty_state_catalogue_link_test.dart'
    as empty_state_catalogue_link_test;
import 'launch_test.dart' as launch_test;
import 'launch_form_test.dart' as launch_form_test;
import 'vm_lifecycle_test.dart' as vm_lifecycle_test;

void main() {
  bulk_actions_test.main();
  catalogue_test.main();
  empty_state_catalogue_link_test.main();
  launch_test.main();
  launch_form_test.main();
  vm_lifecycle_test.main();
}
