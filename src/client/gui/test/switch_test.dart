import 'package:flutter/cupertino.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:multipass_gui/switch.dart' show Switch;

void main() {
  Widget buildSwitch({
    required bool value,
    ValueChanged<bool>? onChanged,
    String label = '',
    bool trailingSwitch = false,
    double size = 25,
    bool enabled = true,
  }) {
    return CupertinoApp(
      home: CupertinoPageScaffold(
        child: Center(
          child: Switch(
            value: value,
            onChanged: onChanged,
            label: label,
            trailingSwitch: trailingSwitch,
            size: size,
            enabled: enabled,
          ),
        ),
      ),
    );
  }

  testWidgets('CupertinoSwitch value is true when value is true',
      (tester) async {
    await tester.pumpWidget(buildSwitch(value: true));
    await tester.pumpAndSettle();

    final cupertinoSwitch =
        tester.widget<CupertinoSwitch>(find.byType(CupertinoSwitch));
    expect(cupertinoSwitch.value, isTrue);
  });

  testWidgets('CupertinoSwitch value is false when value is false',
      (tester) async {
    await tester.pumpWidget(buildSwitch(value: false));
    await tester.pumpAndSettle();

    final cupertinoSwitch =
        tester.widget<CupertinoSwitch>(find.byType(CupertinoSwitch));
    expect(cupertinoSwitch.value, isFalse);
  });

  testWidgets('CupertinoSwitch onChanged is not null when enabled is true',
      (tester) async {
    await tester.pumpWidget(buildSwitch(value: false, onChanged: (_) {}));
    await tester.pumpAndSettle();

    final cupertinoSwitch =
        tester.widget<CupertinoSwitch>(find.byType(CupertinoSwitch));
    expect(cupertinoSwitch.onChanged, isNotNull);
  });

  testWidgets('CupertinoSwitch onChanged is null when enabled is false',
      (tester) async {
    await tester.pumpWidget(
        buildSwitch(value: false, onChanged: (_) {}, enabled: false));
    await tester.pumpAndSettle();

    final cupertinoSwitch =
        tester.widget<CupertinoSwitch>(find.byType(CupertinoSwitch));
    expect(cupertinoSwitch.onChanged, isNull);
  });

  testWidgets('switch appears before label when trailingSwitch is false',
      (tester) async {
    await tester.pumpWidget(buildSwitch(value: false, label: 'My Label'));
    await tester.pumpAndSettle();

    final switchRect = tester.getTopLeft(find.byType(CupertinoSwitch));
    final textRect = tester.getTopLeft(find.text('My Label'));
    expect(switchRect.dx, lessThan(textRect.dx));
  });

  testWidgets('label appears before switch when trailingSwitch is true',
      (tester) async {
    await tester.pumpWidget(
        buildSwitch(value: false, label: 'My Label', trailingSwitch: true));
    await tester.pumpAndSettle();

    final switchRect = tester.getTopLeft(find.byType(CupertinoSwitch));
    final textRect = tester.getTopLeft(find.text('My Label'));
    expect(textRect.dx, lessThan(switchRect.dx));
  });

  testWidgets('tapping switch invokes callback when enabled is true',
      (tester) async {
    bool? receivedValue;
    await tester.pumpWidget(
        buildSwitch(value: false, onChanged: (v) => receivedValue = v));
    await tester.pumpAndSettle();

    await tester.tap(find.byType(CupertinoSwitch));
    await tester.pumpAndSettle();

    expect(receivedValue, isTrue);
  });

  testWidgets('tapping switch does not invoke callback when enabled is false',
      (tester) async {
    var callCount = 0;
    await tester.pumpWidget(buildSwitch(
        value: false, onChanged: (_) => callCount++, enabled: false));
    await tester.pumpAndSettle();

    await tester.tap(find.byType(CupertinoSwitch));
    await tester.pumpAndSettle();

    expect(callCount, equals(0));
  });

  testWidgets('empty label renders an empty Text widget', (tester) async {
    await tester.pumpWidget(buildSwitch(value: false));
    await tester.pumpAndSettle();

    final textWidget = tester.widget<Text>(find.byType(Text));
    expect(textWidget.data, isEmpty);
  });
}
