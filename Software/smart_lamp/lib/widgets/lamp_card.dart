import 'package:flutter/material.dart';

import '../ble/ble_connection_manager.dart';
import '../models/bonded_lamp.dart';

class LampCard extends StatelessWidget {
  final BondedLamp lamp;
  final LampConnectionState connectionState;
  final VoidCallback onTap;
  final VoidCallback onDelete;

  const LampCard({
    super.key,
    required this.lamp,
    required this.connectionState,
    required this.onTap,
    required this.onDelete,
  });

  @override
  Widget build(BuildContext context) {
    final (icon, label, color) = switch (connectionState) {
      LampConnectionState.connected => (Icons.bluetooth_connected, 'Connected', Colors.green),
      LampConnectionState.connecting => (Icons.bluetooth_searching, 'Connecting...', Colors.orange),
      LampConnectionState.disconnected => (Icons.bluetooth_disabled, 'Disconnected', Colors.grey),
    };

    return Card(
      child: ListTile(
        leading: Icon(Icons.lightbulb, color: Theme.of(context).colorScheme.primary),
        title: Text(lamp.name),
        subtitle: Row(
          children: [
            Icon(icon, size: 14, color: color),
            const SizedBox(width: 4),
            Text(label, style: TextStyle(color: color, fontSize: 12)),
          ],
        ),
        trailing: PopupMenuButton<String>(
          onSelected: (value) {
            if (value == 'remove') onDelete();
          },
          itemBuilder: (context) => [
            const PopupMenuItem(value: 'remove', child: Text('Remove')),
          ],
        ),
        onTap: onTap,
      ),
    );
  }
}
