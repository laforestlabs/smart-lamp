import 'package:flutter_test/flutter_test.dart';

import 'package:smart_lamp/main.dart';

void main() {
  testWidgets('App renders smoke test', (WidgetTester tester) async {
    await tester.pumpWidget(const SmartLampApp());
    expect(find.text('Smart Lamp'), findsOneWidget);
  });
}
