import 'package:flutter/material.dart';

import 'general_settings.dart';
import 'usage_settings.dart';
import 'virtualization_settings.dart';

class SettingsScreen extends StatelessWidget {
  static const sidebarKey = 'settings';

  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    const settings = Padding(
      padding: EdgeInsets.only(right: 15),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          GeneralSettings(),
          Divider(height: 60),
          UsageSettings(),
          Divider(height: 60),
          VirtualizationSettings(),
        ],
      ),
    );

    return const Scaffold(
      body: Padding(
        padding: EdgeInsets.symmetric(horizontal: 40, vertical: 40),
        child: SizedBox(
          width: 800,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Settings', style: TextStyle(fontSize: 37)),
              SizedBox(height: 32),
              Expanded(child: SingleChildScrollView(child: settings)),
            ],
          ),
        ),
      ),
    );
  }
}
