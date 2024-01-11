import 'package:flutter/material.dart';

class SettingsScreen extends StatelessWidget {
  static const sidebarKey = 'settings';

  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 3,
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
              TabBar(
                isScrollable: true,
                labelColor: Colors.black,
                labelStyle: TextStyle(fontWeight: FontWeight.bold),
                tabAlignment: TabAlignment.start,
                indicator: BoxDecoration(
                  color: Colors.black12,
                  border: Border(bottom: BorderSide(width: 3)),
                ),
                tabs: [
                  Tab(text: 'General'),
                  Tab(text: 'Usage'),
                  Tab(text: 'Virtualization'),
                ],
              ),
              Divider(height: 0, thickness: 2),
              Expanded(
                child: TabBarView(children: [
                  SingleChildScrollView(child: GeneralSettings()),
                  SingleChildScrollView(child: UsageSettings()),
                  SingleChildScrollView(child: VirtualizationSettings()),
                ]),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class GeneralSettings extends StatelessWidget {
  const GeneralSettings({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: List.generate(50, (index) => Text('$index')),
    );
  }
}

class UsageSettings extends StatelessWidget {
  const UsageSettings({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: List.generate(50, (index) => Text('$index')),
    );
  }
}

class VirtualizationSettings extends StatelessWidget {
  const VirtualizationSettings({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: List.generate(50, (index) => Text('$index')),
    );
  }
}
