import 'dart:async';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

import '../models/auto_config.dart';
import '../models/flame_config.dart';
import '../models/lamp_state.dart';
import '../models/scene.dart';
import '../models/schedule.dart';
import '../models/sensor_data.dart';
import '../models/sync_config.dart';
import 'ble_codec.dart';
import 'ble_service.dart';
import 'ble_uuids.dart';

enum LampConnectionState { disconnected, connecting, connected }

class BleConnectionManager {
  final BleService _bleService;
  String? _deviceId;

  final _connectionStateController =
      StreamController<LampConnectionState>.broadcast();
  Stream<LampConnectionState> get connectionState =>
      _connectionStateController.stream;

  StreamSubscription<ConnectionStateUpdate>? _connectionSub;

  // Initial state read on connect
  LedState? initialLedState;
  ModeFlags? initialModeFlags;
  AutoConfig? initialAutoConfig;
  FlameConfig? initialFlameConfig;
  List<Scene>? initialScenes;
  List<Schedule>? initialSchedules;
  int? initialPirSensitivity;
  String? firmwareVersion;
  SyncConfig? initialSyncConfig;

  // Notification streams
  final _ledStateController = StreamController<LedState>.broadcast();
  Stream<LedState> get ledStateStream => _ledStateController.stream;

  final _sensorDataController = StreamController<SensorData>.broadcast();
  Stream<SensorData> get sensorDataStream => _sensorDataController.stream;

  final _sceneListController = StreamController<List<Scene>>.broadcast();
  Stream<List<Scene>> get sceneListStream => _sceneListController.stream;

  final _scheduleListController = StreamController<List<Schedule>>.broadcast();
  Stream<List<Schedule>> get scheduleListStream =>
      _scheduleListController.stream;

  final _modeFlagsController = StreamController<ModeFlags>.broadcast();
  Stream<ModeFlags> get modeFlagsStream => _modeFlagsController.stream;

  final _syncConfigController = StreamController<SyncConfig>.broadcast();
  Stream<SyncConfig> get syncConfigStream => _syncConfigController.stream;

  final _otaStatusController = StreamController<int>.broadcast();
  Stream<int> get otaStatusStream => _otaStatusController.stream;

  final List<StreamSubscription> _notifySubs = [];

  BleConnectionManager(this._bleService);

  String? get deviceId => _deviceId;

  Future<void> connect(String deviceId) async {
    // Clean up previous connection
    _cancelNotifications();
    _connectionSub?.cancel();
    _clearInitialState();

    _deviceId = deviceId;
    _connectionStateController.add(LampConnectionState.connecting);

    _connectionSub = _bleService.connectToDevice(deviceId).listen(
      (update) async {
        if (update.connectionState == DeviceConnectionState.connected) {
          await _onConnected(deviceId);
          _connectionStateController.add(LampConnectionState.connected);
        } else if (update.connectionState ==
            DeviceConnectionState.disconnected) {
          _cancelNotifications();
          _connectionStateController.add(LampConnectionState.disconnected);
        }
      },
      onError: (e) {
        _connectionStateController.add(LampConnectionState.disconnected);
      },
    );
  }

  Future<void> _onConnected(String deviceId) async {
    // Request MTU
    try {
      await _bleService.requestMtu(deviceId, 512);
    } catch (_) {}

    // Read initial state
    try {
      final ledBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.ledState);
      initialLedState = BleCodec.decodeLedState(ledBytes);

      final modeBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.mode);
      initialModeFlags = BleCodec.decodeModeFlags(modeBytes);

      final autoBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.autoConfig);
      initialAutoConfig = BleCodec.decodeAutoConfig(autoBytes);

      final flameBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.flameConfig);
      initialFlameConfig = BleCodec.decodeFlameConfig(flameBytes);

      final sceneBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.sceneList);
      initialScenes = BleCodec.decodeSceneList(sceneBytes);

      final schedBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.scheduleList);
      initialSchedules = BleCodec.decodeScheduleList(schedBytes);

      final pirSensBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.pirSensitivity);
      initialPirSensitivity = BleCodec.decodePirSensitivity(pirSensBytes);

      final infoBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.deviceInfo);
      firmwareVersion = BleCodec.decodeDeviceInfo(infoBytes);

      final syncBytes =
          await _bleService.readCharacteristic(deviceId, BleUuids.syncConfig);
      initialSyncConfig = BleCodec.decodeSyncConfig(syncBytes);
    } catch (e) {
      // Non-fatal — we'll work with whatever we got
    }

    // Push initial state through streams so existing notifiers pick it up
    if (initialLedState != null) _ledStateController.add(initialLedState!);
    if (initialModeFlags != null) _modeFlagsController.add(initialModeFlags!);
    if (initialScenes != null) _sceneListController.add(initialScenes!);
    if (initialSchedules != null) _scheduleListController.add(initialSchedules!);
    if (initialSyncConfig != null) _syncConfigController.add(initialSyncConfig!);

    // Subscribe to notifications
    _notifySubs.add(
      _bleService
          .subscribeToCharacteristic(deviceId, BleUuids.ledState)
          .listen((bytes) => _ledStateController.add(BleCodec.decodeLedState(bytes))),
    );
    _notifySubs.add(
      _bleService
          .subscribeToCharacteristic(deviceId, BleUuids.sensorData)
          .listen((bytes) =>
              _sensorDataController.add(BleCodec.decodeSensorData(bytes))),
    );
    _notifySubs.add(
      _bleService
          .subscribeToCharacteristic(deviceId, BleUuids.sceneList)
          .listen((bytes) async {
        // Re-read the full scene list on notification
        try {
          final full = await _bleService.readCharacteristic(
              deviceId, BleUuids.sceneList);
          _sceneListController.add(BleCodec.decodeSceneList(full));
        } catch (_) {}
      }),
    );
    _notifySubs.add(
      _bleService
          .subscribeToCharacteristic(deviceId, BleUuids.scheduleList)
          .listen((bytes) async {
        try {
          final full = await _bleService.readCharacteristic(
              deviceId, BleUuids.scheduleList);
          _scheduleListController.add(BleCodec.decodeScheduleList(full));
        } catch (_) {}
      }),
    );
    _notifySubs.add(
      _bleService
          .subscribeToCharacteristic(deviceId, BleUuids.otaControl)
          .listen((bytes) {
        if (bytes.isNotEmpty) _otaStatusController.add(bytes[0]);
      }),
    );
  }

  void _cancelNotifications() {
    for (final sub in _notifySubs) {
      sub.cancel();
    }
    _notifySubs.clear();
  }

  void _clearInitialState() {
    initialLedState = null;
    initialModeFlags = null;
    initialAutoConfig = null;
    initialFlameConfig = null;
    initialScenes = null;
    initialSchedules = null;
    initialPirSensitivity = null;
    firmwareVersion = null;
    initialSyncConfig = null;
  }

  void disconnect() {
    _connectionSub?.cancel();
    _cancelNotifications();
    _connectionStateController.add(LampConnectionState.disconnected);
    _deviceId = null;
  }

  void dispose() {
    disconnect();
    _connectionStateController.close();
    _ledStateController.close();
    _modeFlagsController.close();
    _sensorDataController.close();
    _sceneListController.close();
    _scheduleListController.close();
    _syncConfigController.close();
    _otaStatusController.close();
  }
}
