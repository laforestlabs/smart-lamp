import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../ble/ble_connection_manager.dart';
import '../providers/ble_provider.dart';
import '../providers/device_list_provider.dart';
import '../widgets/lamp_card.dart';

class HomeScreen extends ConsumerStatefulWidget {
  const HomeScreen({super.key});

  @override
  ConsumerState<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends ConsumerState<HomeScreen> {
  bool _autoConnectAttempted = false;

  @override
  void initState() {
    super.initState();
    // Auto-connect on next frame so providers are ready
    WidgetsBinding.instance.addPostFrameCallback((_) => _tryAutoConnect());
  }

  void _tryAutoConnect() {
    if (_autoConnectAttempted) return;
    _autoConnectAttempted = true;

    final lamps = ref.read(deviceListProvider);
    final connManager = ref.read(connectionManagerProvider);
    if (lamps.isNotEmpty && connManager.deviceId == null) {
      connManager.connect(lamps.first.deviceId);
    }
  }

  @override
  Widget build(BuildContext context) {
    final ref = this.ref;
    final lamps = ref.watch(deviceListProvider);
    final connState = ref.watch(connectionStateProvider);
    final connManager = ref.read(connectionManagerProvider);

    // Auto-navigate to control screen once connected
    ref.listen<AsyncValue<LampConnectionState>>(connectionStateProvider,
        (prev, next) {
      final prevState = prev?.valueOrNull;
      final nextState = next.valueOrNull;
      if (prevState != LampConnectionState.connected &&
          nextState == LampConnectionState.connected &&
          connManager.deviceId != null) {
        context.push('/control/${connManager.deviceId}');
      }
    });

    return Scaffold(
      appBar: AppBar(
        title: const Text('Smart Lamp'),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () => context.push('/settings'),
          ),
        ],
      ),
      body: lamps.isEmpty
          ? Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.lightbulb_outline,
                      size: 64, color: Theme.of(context).colorScheme.primary),
                  const SizedBox(height: 16),
                  const Text('No lamps paired yet'),
                  const SizedBox(height: 8),
                  FilledButton.icon(
                    onPressed: () => context.push('/pairing'),
                    icon: const Icon(Icons.add),
                    label: const Text('Add Lamp'),
                  ),
                  const SizedBox(height: 48),
                  Text(
                    'by Laforest Labs',
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: Theme.of(context)
                              .colorScheme
                              .onSurface
                              .withAlpha(100),
                        ),
                  ),
                ],
              ),
            )
          : ListView.builder(
              padding: const EdgeInsets.all(8),
              itemCount: lamps.length,
              itemBuilder: (context, index) {
                final lamp = lamps[index];
                final currentState = connState.whenOrNull(
                  data: (s) => connManager.deviceId == lamp.deviceId
                      ? s
                      : LampConnectionState.disconnected,
                ) ?? LampConnectionState.disconnected;

                return LampCard(
                  lamp: lamp,
                  connectionState: currentState,
                  onTap: () {
                    if (currentState == LampConnectionState.connected) {
                      context.push('/control/${lamp.deviceId}');
                    } else {
                      connManager.connect(lamp.deviceId);
                    }
                  },
                  onDelete: () {
                    ref.read(deviceListProvider.notifier).removeLamp(lamp.deviceId);
                  },
                );
              },
            ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => context.push('/pairing'),
        child: const Icon(Icons.add),
      ),
    );
  }
}
