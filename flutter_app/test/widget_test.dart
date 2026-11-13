import 'package:flutter_test/flutter_test.dart';
import 'package:switch_hud/main.dart';
import 'package:switch_hud/services/sysbotbase_client.dart';

void main() {
  testWidgets('SwitchHudApp builds smoke test', (WidgetTester tester) async {
    final client = SysBotbaseClient();
    await tester.pumpWidget(SwitchHudApp(client: client));
    expect(find.text('SWITCH HUD'), findsOneWidget);
  });
}
