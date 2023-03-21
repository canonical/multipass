import 'generated/multipass.pbgrpc.dart';

typedef Status = InstanceStatus_Status;

extension IterableExtension on Iterable<String> {
  String joinWithAnd() {
    final n = length;
    if (n == 0) {
      return '';
    }

    if (n == 1) {
      return first;
    }

    return '${take(n - 1).join(', ')} and $last';
  }
}
