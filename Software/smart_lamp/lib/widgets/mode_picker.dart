import 'package:flutter/material.dart';

import '../models/lamp_state.dart';

class ModeToggles extends StatelessWidget {
  final ModeFlags flags;
  final ValueChanged<bool> onAutoChanged;
  final ValueChanged<bool> onFlameChanged;

  const ModeToggles({
    super.key,
    required this.flags,
    required this.onAutoChanged,
    required this.onFlameChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Column(
        children: [
          SwitchListTile(
            title: const Text('Auto Mode'),
            subtitle: const Text('Motion-activated on/off'),
            secondary: const Icon(Icons.sensors),
            value: flags.autoEnabled,
            onChanged: onAutoChanged,
          ),
          SwitchListTile(
            title: const Text('Flame Effect'),
            subtitle: const Text('Animated candle flame'),
            secondary: const Icon(Icons.local_fire_department),
            value: flags.flameEnabled,
            onChanged: onFlameChanged,
          ),
        ],
      ),
    );
  }
}
