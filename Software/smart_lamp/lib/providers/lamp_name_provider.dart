import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import 'ble_provider.dart';

class LampNameNotifier extends StateNotifier<String> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  StreamSubscription? _nameSub;

  LampNameNotifier(this._bleService, this._connManager)
      : super(_connManager.initialLampName ?? '') {
    _nameSub = _connManager.lampNameStream.listen((name) {
      state = name;
    });
  }

  Future<void> setName(String name) async {
    state = name;
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.lampName,
        BleCodec.encodeLampName(name),
      );
    } catch (_) {}
  }

  @override
  void dispose() {
    _nameSub?.cancel();
    super.dispose();
  }
}

final lampNameProvider =
    StateNotifierProvider<LampNameNotifier, String>((ref) {
  return LampNameNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
