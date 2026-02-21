import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/schedule.dart';
import '../providers/scene_provider.dart';
import '../providers/schedule_provider.dart';
import '../widgets/day_of_week_selector.dart';
import '../widgets/schedule_tile.dart';

class SchedulesScreen extends ConsumerWidget {
  const SchedulesScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final schedules = ref.watch(scheduleListProvider);
    final scenes = ref.watch(sceneListProvider);
    final sceneNames = scenes.map((s) => s.name).toList();

    return Scaffold(
      appBar: AppBar(title: const Text('Schedules')),
      body: schedules.isEmpty
          ? const Center(child: Text('No schedules set'))
          : ListView.builder(
              itemCount: schedules.length,
              itemBuilder: (context, index) {
                final sched = schedules[index];
                return ScheduleTile(
                  schedule: sched,
                  sceneNames: sceneNames,
                  onToggle: (enabled) {
                    ref.read(scheduleListProvider.notifier).saveSchedule(
                          sched.copyWith(enabled: enabled),
                        );
                  },
                  onTap: () => _showEditDialog(context, ref, sched, scenes),
                  onDelete: () {
                    ref.read(scheduleListProvider.notifier).deleteSchedule(sched.index);
                  },
                );
              },
            ),
      floatingActionButton: FloatingActionButton(
        onPressed: () {
          final nextIndex = schedules.isEmpty
              ? 0
              : schedules.map((s) => s.index).reduce((a, b) => a > b ? a : b) + 1;
          _showEditDialog(
            context,
            ref,
            Schedule(index: nextIndex),
            scenes,
          );
        },
        child: const Icon(Icons.add),
      ),
    );
  }

  void _showEditDialog(
    BuildContext context,
    WidgetRef ref,
    Schedule schedule,
    List schedules,
  ) {
    var dayMask = schedule.dayMask;
    var hour = schedule.hour;
    var minute = schedule.minute;
    var sceneIndex = schedule.sceneIndex;

    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (ctx) {
        return StatefulBuilder(
          builder: (ctx, setState) {
            return Padding(
              padding: EdgeInsets.only(
                left: 16,
                right: 16,
                top: 24,
                bottom: MediaQuery.of(ctx).viewInsets.bottom + 24,
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Edit Schedule',
                      style: Theme.of(ctx).textTheme.titleLarge),
                  const SizedBox(height: 16),
                  DayOfWeekSelector(
                    dayMask: dayMask,
                    onChanged: (v) => setState(() => dayMask = v),
                  ),
                  const SizedBox(height: 16),
                  ListTile(
                    leading: const Icon(Icons.access_time),
                    title: Text(
                        '${hour.toString().padLeft(2, '0')}:${minute.toString().padLeft(2, '0')}'),
                    onTap: () async {
                      final time = await showTimePicker(
                        context: ctx,
                        initialTime: TimeOfDay(hour: hour, minute: minute),
                      );
                      if (time != null) {
                        setState(() {
                          hour = time.hour;
                          minute = time.minute;
                        });
                      }
                    },
                  ),
                  const SizedBox(height: 16),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.end,
                    children: [
                      TextButton(
                        onPressed: () => Navigator.pop(ctx),
                        child: const Text('Cancel'),
                      ),
                      const SizedBox(width: 8),
                      FilledButton(
                        onPressed: () {
                          ref.read(scheduleListProvider.notifier).saveSchedule(
                                schedule.copyWith(
                                  dayMask: dayMask,
                                  hour: hour,
                                  minute: minute,
                                  sceneIndex: sceneIndex,
                                ),
                              );
                          Navigator.pop(ctx);
                        },
                        child: const Text('Save'),
                      ),
                    ],
                  ),
                ],
              ),
            );
          },
        );
      },
    );
  }
}
