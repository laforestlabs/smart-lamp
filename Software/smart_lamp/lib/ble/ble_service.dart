import 'dart:async';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

import 'ble_uuids.dart';

class BleService {
  final FlutterReactiveBle _ble;

  BleService(this._ble);

  Stream<DiscoveredDevice> scanForLamps() {
    return _ble.scanForDevices(
      withServices: [BleUuids.serviceUuid],
      scanMode: ScanMode.lowLatency,
    );
  }

  Stream<ConnectionStateUpdate> connectToDevice(String deviceId) {
    return _ble.connectToDevice(
      id: deviceId,
      connectionTimeout: const Duration(seconds: 10),
    );
  }

  Future<int> requestMtu(String deviceId, int mtu) {
    return _ble.requestMtu(deviceId: deviceId, mtu: mtu);
  }

  Future<List<int>> readCharacteristic(String deviceId, Uuid charUuid) {
    return _ble.readCharacteristic(BleUuids.chr(deviceId, charUuid));
  }

  Future<void> writeCharacteristic(
      String deviceId, Uuid charUuid, List<int> value) {
    return _ble.writeCharacteristicWithResponse(
      BleUuids.chr(deviceId, charUuid),
      value: value,
    );
  }

  Future<void> writeWithoutResponse(
      String deviceId, Uuid charUuid, List<int> value) {
    return _ble.writeCharacteristicWithoutResponse(
      BleUuids.chr(deviceId, charUuid),
      value: value,
    );
  }

  Stream<List<int>> subscribeToCharacteristic(String deviceId, Uuid charUuid) {
    return _ble.subscribeToCharacteristic(BleUuids.chr(deviceId, charUuid));
  }
}
