import 'package:flutter/material.dart';
import 'services/sysbotbase_client.dart';
import 'ui/hud_home.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  final client = SysBotbaseClient()..init();
  runApp(SwitchHudApp(client: client));
}

class SwitchHudApp extends StatelessWidget {
  final SysBotbaseClient client;
  const SwitchHudApp({super.key, required this.client});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Switch HUD',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(useMaterial3: true),
      home: HudHome(client: client),
    );
  }
}
