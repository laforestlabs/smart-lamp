import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/sync_config_provider.dart';

class GroupSyncScreen extends ConsumerWidget {
  const GroupSyncScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final syncConfig = ref.watch(syncConfigProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Group Sync')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Text(
                'Lamps with the same Group ID will sync their on/off state, '
                'brightness, and colour in real time.\n\n'
                'Set Group ID to Off to disable sync.',
                style: Theme.of(context).textTheme.bodyMedium,
              ),
            ),
          ),
          const SizedBox(height: 16),
          ListTile(
            leading: const Icon(Icons.wifi),
            title: const Text('Lamp WiFi MAC'),
            subtitle: Text(
              syncConfig.wifiMac.isNotEmpty ? syncConfig.wifiMac : 'Unknown',
            ),
          ),
          const Divider(),
          ListTile(
            leading: const Icon(Icons.group_work),
            title: const Text('Group ID'),
            subtitle: Text(
              syncConfig.isEnabled
                  ? 'Group ${syncConfig.groupId}'
                  : 'Disabled',
            ),
            trailing: DropdownButton<int>(
              value: syncConfig.groupId,
              underline: const SizedBox(),
              items: [
                const DropdownMenuItem(value: 0, child: Text('Off')),
                for (int i = 1; i <= 10; i++)
                  DropdownMenuItem(value: i, child: Text('$i')),
              ],
              onChanged: (v) {
                if (v != null) {
                  ref.read(syncConfigProvider.notifier).setGroupId(v);
                }
              },
            ),
          ),
          const Divider(),
          if (syncConfig.isEnabled)
            ListTile(
              leading: const Icon(Icons.sync, color: Colors.green),
              title: Text('Syncing with Group ${syncConfig.groupId}'),
              subtitle: const Text(
                'Changes will propagate to all group members',
              ),
            ),
        ],
      ),
    );
  }
}
