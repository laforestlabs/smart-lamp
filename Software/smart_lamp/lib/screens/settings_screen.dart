import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/simulation_provider.dart';

class SettingsScreen extends ConsumerWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final simMode = ref.watch(simulationModeProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: ListView(
        children: [
          SwitchListTile(
            title: const Text('Simulation Mode'),
            subtitle:
                const Text('Use a simulated lamp for UI development'),
            value: simMode,
            onChanged: (_) async {
              await ref.read(simulationModeProvider.notifier).toggle();
              if (context.mounted) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                      content: Text('Restart app to apply')),
                );
              }
            },
          ),
        ],
      ),
    );
  }
}
