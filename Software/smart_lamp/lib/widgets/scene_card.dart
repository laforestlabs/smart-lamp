import 'package:flutter/material.dart';

import '../models/lamp_state.dart';
import '../models/scene.dart';

class SceneCard extends StatelessWidget {
  final Scene scene;
  final VoidCallback onApply;
  final VoidCallback onDelete;

  const SceneCard({
    super.key,
    required this.scene,
    required this.onApply,
    required this.onDelete,
  });

  @override
  Widget build(BuildContext context) {
    return Dismissible(
      key: ValueKey(scene.index),
      direction: DismissDirection.endToStart,
      background: Container(
        alignment: Alignment.centerRight,
        padding: const EdgeInsets.only(right: 16),
        color: Theme.of(context).colorScheme.error,
        child: const Icon(Icons.delete, color: Colors.white),
      ),
      onDismissed: (_) => onDelete(),
      child: ListTile(
        leading: Container(
          width: 40,
          height: 40,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(8),
            gradient: LinearGradient(
              colors: [
                Color.lerp(Colors.black, Colors.amber, scene.warm / 255)!,
                Color.lerp(Colors.black, Colors.white, scene.neutral / 255)!,
                Color.lerp(Colors.black, Colors.lightBlue, scene.cool / 255)!,
              ],
            ),
          ),
        ),
        title: Text(scene.name),
        subtitle: Row(
          children: [
            Text('${(scene.master / 2.55).round()}% brightness'),
            if (ModeFlags.fromByte(scene.modeFlags).flameEnabled) ...[
              const SizedBox(width: 6),
              Icon(Icons.local_fire_department, size: 14, color: Colors.orange.shade400),
            ],
            if (ModeFlags.fromByte(scene.modeFlags).autoEnabled) ...[
              const SizedBox(width: 4),
              Icon(Icons.sensors, size: 14, color: Colors.blue.shade300),
            ],
          ],
        ),
        trailing: const Icon(Icons.play_arrow),
        onTap: onApply,
      ),
    );
  }
}
