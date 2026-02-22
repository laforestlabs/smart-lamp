import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_connection_manager.dart';
import '../ble/ble_service.dart';
import '../ble/simulated_ble_service.dart';
import '../ble/simulated_connection_manager.dart';
import 'simulation_provider.dart';

final flutterReactiveBleProvider = Provider<FlutterReactiveBle>((ref) {
  return FlutterReactiveBle();
});

final bleServiceProvider = Provider<BleService>((ref) {
  if (ref.watch(simulationModeProvider)) {
    return SimulatedBleService();
  }
  return BleService(ref.watch(flutterReactiveBleProvider));
});

final connectionManagerProvider = Provider<BleConnectionManager>((ref) {
  final bleService = ref.watch(bleServiceProvider);
  final simMode = ref.watch(simulationModeProvider);
  final BleConnectionManager manager;
  if (simMode) {
    manager = SimulatedBleConnectionManager(bleService);
  } else {
    manager = BleConnectionManager(bleService);
  }
  ref.onDispose(() => manager.dispose());
  return manager;
});

final connectionStateProvider =
    StreamProvider<LampConnectionState>((ref) {
  return ref.watch(connectionManagerProvider).connectionState;
});

final scanResultsProvider =
    StreamProvider.autoDispose<DiscoveredDevice>((ref) {
  return ref.watch(bleServiceProvider).scanForLamps();
});
