import 'package:flutter/material.dart';

import '../models/lamp_state.dart';

class ModePicker extends StatelessWidget {
  final LampMode selected;
  final ValueChanged<LampMode> onChanged;

  const ModePicker({
    super.key,
    required this.selected,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: SegmentedButton<LampMode>(
        segments: const [
          ButtonSegment(
            value: LampMode.manual,
            label: Text('Manual'),
            icon: Icon(Icons.tune),
          ),
          ButtonSegment(
            value: LampMode.auto,
            label: Text('Auto'),
            icon: Icon(Icons.sensors),
          ),
          ButtonSegment(
            value: LampMode.flame,
            label: Text('Flame'),
            icon: Icon(Icons.local_fire_department),
          ),
        ],
        selected: {selected},
        onSelectionChanged: (set) => onChanged(set.first),
      ),
    );
  }
}
