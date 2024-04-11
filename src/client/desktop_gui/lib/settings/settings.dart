import 'package:flutter/material.dart';

import 'general_settings.dart';
import 'usage_settings.dart';
import 'virtualization_settings.dart';

class SettingsScreen extends StatelessWidget {
  static const sidebarKey = 'settings';

  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    const tabTitles = [
      Tab(text: 'General'),
      Tab(text: 'Usage'),
      Tab(text: 'Virtualization'),
    ];

    const tabContents = [
      SingleChildScrollView(child: GeneralSettings()),
      SingleChildScrollView(child: UsageSettings()),
      SingleChildScrollView(child: VirtualizationSettings()),
    ];

    return DefaultTabController(
      length: tabContents.length,
      child: Scaffold(
        body: Padding(
          padding:
              const EdgeInsets.symmetric(horizontal: 140).copyWith(top: 40),
          child: const Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Settings', style: TextStyle(fontSize: 37)),
              SizedBox(height: 32),
              TabBar(isScrollable: true, tabs: tabTitles),
              Divider(height: 0, thickness: 2),
              Expanded(child: TabBarView(children: tabContents)),
            ],
          ),
        ),
      ),
    );
  }
}
