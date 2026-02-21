import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_connection_manager.dart';
import '../ble/ble_service.dart';

final flutterReactiveBleProvider = Provider<FlutterReactiveBle>((ref) {
  return FlutterReactiveBle();
});

final bleServiceProvider = Provider<BleService>((ref) {
  return BleService(ref.watch(flutterReactiveBleProvider));
});

final connectionManagerProvider = Provider<BleConnectionManager>((ref) {
  final manager = BleConnectionManager(ref.watch(bleServiceProvider));
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
