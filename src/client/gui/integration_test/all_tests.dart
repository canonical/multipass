// Single entry point that runs all integration tests in one build+process.
import 'empty_state_catalogue_link_test.dart'
    as empty_state_catalogue_link_test;
import 'catalogue_test.dart' as catalogue_test;

void main() {
  catalogue_test.main();
  empty_state_catalogue_link_test.main();
}
