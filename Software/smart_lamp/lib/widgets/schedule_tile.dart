import 'package:flutter/material.dart';

import '../models/schedule.dart';

class ScheduleTile extends StatelessWidget {
  final Schedule schedule;
  final List<String> sceneNames;
  final ValueChanged<bool> onToggle;
  final VoidCallback onTap;
  final VoidCallback onDelete;

  const ScheduleTile({
    super.key,
    required this.schedule,
    required this.sceneNames,
    required this.onToggle,
    required this.onTap,
    required this.onDelete,
  });

  static const _dayLabels = ['M', 'T', 'W', 'T', 'F', 'S', 'S'];

  @override
  Widget build(BuildContext context) {
    final time =
        '${schedule.hour.toString().padLeft(2, '0')}:${schedule.minute.toString().padLeft(2, '0')}';
    final action = schedule.sceneIndex == 0xFF
        ? 'Turn off'
        : (schedule.sceneIndex < sceneNames.length
            ? sceneNames[schedule.sceneIndex]
            : 'Scene ${schedule.sceneIndex}');

    return Dismissible(
      key: ValueKey(schedule.index),
      direction: DismissDirection.endToStart,
      background: Container(
        alignment: Alignment.centerRight,
        padding: const EdgeInsets.only(right: 16),
        color: Theme.of(context).colorScheme.error,
        child: const Icon(Icons.delete, color: Colors.white),
      ),
      onDismissed: (_) => onDelete(),
      child: ListTile(
        title: Row(
          children: [
            Text(time, style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(width: 12),
            Text(action),
          ],
        ),
        subtitle: Padding(
          padding: const EdgeInsets.only(top: 4),
          child: Row(
            children: List.generate(7, (i) {
              final active = schedule.isActiveOn(i);
              return Padding(
                padding: const EdgeInsets.only(right: 4),
                child: CircleAvatar(
                  radius: 12,
                  backgroundColor: active
                      ? Theme.of(context).colorScheme.primary
                      : Theme.of(context).colorScheme.surfaceContainerHighest,
                  child: Text(
                    _dayLabels[i],
                    style: TextStyle(
                      fontSize: 10,
                      color: active ? Colors.black : Colors.grey,
                    ),
                  ),
                ),
              );
            }),
          ),
        ),
        trailing: Switch(
          value: schedule.enabled,
          onChanged: onToggle,
        ),
        onTap: onTap,
      ),
    );
  }
}
