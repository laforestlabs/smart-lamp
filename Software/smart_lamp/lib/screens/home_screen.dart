import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../ble/ble_connection_manager.dart';
import '../models/bonded_lamp.dart';
import '../providers/ble_provider.dart';
import '../providers/device_list_provider.dart';
import '../widgets/lamp_card.dart';

class HomeScreen extends ConsumerStatefulWidget {
  const HomeScreen({super.key});

  @override
  ConsumerState<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends ConsumerState<HomeScreen> {
  bool _autoConnecting = false;
  int _autoConnectIndex = 0;
  List<BondedLamp> _autoConnectOrder = [];

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _tryAutoConnect());
  }

  void _tryAutoConnect() {
    final lamps = ref.read(deviceListProvider);
    final connManager = ref.read(connectionManagerProvider);
    if (lamps.isEmpty || connManager.deviceId != null) return;

    // Sort by lastConnected descending (most recent first, nulls last)
    _autoConnectOrder = List.of(lamps)
      ..sort((a, b) {
        if (a.lastConnected == null && b.lastConnected == null) return 0;
        if (a.lastConnected == null) return 1;
        if (b.lastConnected == null) return -1;
        return b.lastConnected!.compareTo(a.lastConnected!);
      });

    _autoConnectIndex = 0;
    _autoConnecting = true;
    connManager.connect(_autoConnectOrder[0].deviceId);
  }

  void _onAutoConnectFailed() {
    if (!_autoConnecting) return;
    _autoConnectIndex++;
    if (_autoConnectIndex >= _autoConnectOrder.length) {
      _autoConnecting = false;
      return;
    }
    final connManager = ref.read(connectionManagerProvider);
    connManager.connect(_autoConnectOrder[_autoConnectIndex].deviceId);
  }

  @override
  Widget build(BuildContext context) {
    final ref = this.ref;
    final lamps = ref.watch(deviceListProvider);
    final connState = ref.watch(connectionStateProvider);
    final connManager = ref.read(connectionManagerProvider);

    ref.listen<AsyncValue<LampConnectionState>>(connectionStateProvider,
        (prev, next) {
      final prevState = prev?.valueOrNull;
      final nextState = next.valueOrNull;

      if (nextState == LampConnectionState.connected &&
          connManager.deviceId != null) {
        // Record last-connected timestamp
        ref
            .read(deviceListProvider.notifier)
            .updateLastConnected(connManager.deviceId!);
        _autoConnecting = false;

        // Auto-navigate to control screen
        if (prevState != LampConnectionState.connected) {
          context.push('/control/${connManager.deviceId}');
        }
      } else if (prevState == LampConnectionState.connecting &&
          nextState == LampConnectionState.disconnected &&
          _autoConnecting) {
        // Connection failed during auto-connect — try next lamp
        _onAutoConnectFailed();
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
                    _autoConnecting = false; // Cancel auto-connect on manual tap
                    if (currentState == LampConnectionState.connected) {
                      context.push('/control/${lamp.deviceId}');
                    } else {
                      if (connManager.deviceId != null &&
                          connManager.deviceId != lamp.deviceId) {
                        connManager.disconnect();
                      }
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
