import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

import '../models/auto_config.dart';
import '../models/flame_config.dart';
import 'ble_codec.dart';
import 'ble_service.dart';
import 'ble_uuids.dart';

const _simDeviceId = 'sim-0000';
const _simDeviceName = 'SmartLamp-SIM';

class SimulatedBleService extends BleService {
  final Map<String, List<int>> _store = {};

  SimulatedBleService() : super(FlutterReactiveBle()) {
    _store[BleUuids.ledState.toString()] = [200, 100, 50, 255];
    _store[BleUuids.mode.toString()] = [0];
    _store[BleUuids.autoConfig.toString()] =
        BleCodec.encodeAutoConfig(const AutoConfig());
    _store[BleUuids.flameConfig.toString()] =
        BleCodec.encodeFlameConfig(const FlameConfig());
    _store[BleUuids.sceneList.toString()] = _encodeSampleScenes();
    _store[BleUuids.scheduleList.toString()] = [0];
    _store[BleUuids.sensorData.toString()] = [30, 0, 0];
    _store[BleUuids.deviceInfo.toString()] = utf8.encode('SIM 1.0.0');
  }

  static List<int> _encodeSampleScenes() {
    final reading = utf8.encode('Reading');
    final relax = utf8.encode('Relax');
    return [
      2, // count
      0, reading.length, ...reading, 255, 80, 20, 200, // scene 0
      1, relax.length, ...relax, 180, 60, 10, 140, // scene 1
    ];
  }

  @override
  Stream<DiscoveredDevice> scanForLamps() async* {
    await Future.delayed(const Duration(milliseconds: 500));
    yield DiscoveredDevice(
      id: _simDeviceId,
      name: _simDeviceName,
      serviceData: const {},
      serviceUuids: [BleUuids.serviceUuid],
      manufacturerData: Uint8List(0),
      rssi: -50,
      connectable: Connectable.available,
    );
  }

  @override
  Stream<ConnectionStateUpdate> connectToDevice(String deviceId) async* {
    await Future.delayed(const Duration(milliseconds: 300));
    yield ConnectionStateUpdate(
      deviceId: deviceId,
      connectionState: DeviceConnectionState.connected,
      failure: null,
    );
  }

  @override
  Future<int> requestMtu(String deviceId, int mtu) async => 512;

  @override
  Future<List<int>> readCharacteristic(String deviceId, Uuid charUuid) async {
    return _store[charUuid.toString()] ?? [];
  }

  @override
  Future<void> writeCharacteristic(
      String deviceId, Uuid charUuid, List<int> value) async {
    _store[charUuid.toString()] = value;
  }

  @override
  Future<void> writeWithoutResponse(
      String deviceId, Uuid charUuid, List<int> value) async {
    _store[charUuid.toString()] = value;
  }

  @override
  Stream<List<int>> subscribeToCharacteristic(
      String deviceId, Uuid charUuid) {
    return const Stream.empty();
  }
}
