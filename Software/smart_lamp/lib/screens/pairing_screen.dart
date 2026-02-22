import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:permission_handler/permission_handler.dart';

import '../ble/ble_connection_manager.dart';
import '../models/bonded_lamp.dart';
import '../providers/ble_provider.dart';
import '../providers/device_list_provider.dart';

class PairingScreen extends ConsumerStatefulWidget {
  const PairingScreen({super.key});

  @override
  ConsumerState<PairingScreen> createState() => _PairingScreenState();
}

class _PairingScreenState extends ConsumerState<PairingScreen> {
  final _discovered = <DiscoveredDevice>[];
  StreamSubscription? _scanSub;
  bool _scanning = false;
  bool _connecting = false;

  @override
  void initState() {
    super.initState();
    _requestPermissionsAndScan();
  }

  Future<void> _requestPermissionsAndScan() async {
    if (Platform.isAndroid) {
      final statuses = await [
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.locationWhenInUse,
      ].request();
      final denied = statuses.values.any((s) => s.isDenied || s.isPermanentlyDenied);
      if (denied && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('BLE permissions are required to scan')),
        );
        return;
      }
    }
    _startScan();
  }

  void _startScan() {
    setState(() {
      _discovered.clear();
      _scanning = true;
    });
    _scanSub = ref.read(bleServiceProvider).scanForLamps().listen((device) {
      if (!_discovered.any((d) => d.id == device.id)) {
        setState(() => _discovered.add(device));
      }
    });
    // Auto-stop after 30 seconds
    Future.delayed(const Duration(seconds: 30), () {
      if (mounted && _scanning) _stopScan();
    });
  }

  void _stopScan() {
    _scanSub?.cancel();
    setState(() => _scanning = false);
  }

  Future<void> _connectTo(DiscoveredDevice device) async {
    _stopScan();
    setState(() => _connecting = true);

    final connManager = ref.read(connectionManagerProvider);

    // Listen before calling connect so we don't miss the event on broadcast streams
    final stateFuture = connManager.connectionState
        .firstWhere((s) =>
            s == LampConnectionState.connected ||
            s == LampConnectionState.disconnected)
        .timeout(const Duration(seconds: 15));

    connManager.connect(device.id);

    try {
      final state = await stateFuture;

      if (state == LampConnectionState.connected) {
        // Save bonded lamp
        await ref.read(deviceListProvider.notifier).addLamp(
              BondedLamp(deviceId: device.id, name: device.name),
            );
        if (mounted) {
          context.go('/control/${device.id}');
        }
      } else {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Connection failed')),
          );
        }
      }
    } catch (_) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Connection timed out')),
        );
      }
    }

    if (mounted) setState(() => _connecting = false);
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Add Lamp')),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Row(
                  children: [
                    Icon(Icons.touch_app,
                        color: Theme.of(context).colorScheme.primary),
                    const SizedBox(width: 12),
                    const Expanded(
                      child: Text(
                        'Hold the touch pad on your lamp for 3 seconds to start pairing.',
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
          if (_scanning)
            const Padding(
              padding: EdgeInsets.symmetric(horizontal: 16),
              child: LinearProgressIndicator(),
            ),
          if (_connecting)
            const Padding(
              padding: EdgeInsets.all(32),
              child: Column(
                children: [
                  CircularProgressIndicator(),
                  SizedBox(height: 16),
                  Text('Connecting & bonding...'),
                ],
              ),
            ),
          Expanded(
            child: _discovered.isEmpty && _scanning
                ? const Center(child: Text('Scanning for lamps...'))
                : ListView.builder(
                    itemCount: _discovered.length,
                    itemBuilder: (context, index) {
                      final device = _discovered[index];
                      return ListTile(
                        leading: const Icon(Icons.lightbulb),
                        title: Text(device.name.isNotEmpty
                            ? device.name
                            : 'Unknown Device'),
                        subtitle: Text(device.id),
                        trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                        onTap: _connecting ? null : () => _connectTo(device),
                      );
                    },
                  ),
          ),
          if (!_scanning && !_connecting)
            Padding(
              padding: const EdgeInsets.all(16),
              child: OutlinedButton.icon(
                onPressed: _startScan,
                icon: const Icon(Icons.refresh),
                label: const Text('Scan Again'),
              ),
            ),
        ],
      ),
    );
  }
}
