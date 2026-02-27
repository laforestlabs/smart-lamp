import 'dart:async';
import 'dart:math';

import '../models/auto_config.dart';
import '../models/flame_config.dart';
import '../models/lamp_state.dart';
import '../models/scene.dart';
import '../models/schedule.dart';
import '../models/sensor_data.dart';
import '../models/sync_config.dart';
import 'ble_connection_manager.dart';
import 'ble_service.dart';

class SimulatedBleConnectionManager implements BleConnectionManager {
  // ignore: unused_field
  final BleService _bleService;

  Timer? _sensorTimer;
  final _random = Random();

  String? _deviceId;

  final _connectionStateCtrl =
      StreamController<LampConnectionState>.broadcast();
  final _ledStateCtrl = StreamController<LedState>.broadcast();
  final _sensorDataCtrl = StreamController<SensorData>.broadcast();
  final _sceneListCtrl = StreamController<List<Scene>>.broadcast();
  final _scheduleListCtrl = StreamController<List<Schedule>>.broadcast();
  final _otaStatusCtrl = StreamController<int>.broadcast();

  @override
  LedState? initialLedState;
  @override
  ModeFlags? initialModeFlags;
  @override
  AutoConfig? initialAutoConfig;
  @override
  FlameConfig? initialFlameConfig;
  @override
  List<Scene>? initialScenes;
  @override
  List<Schedule>? initialSchedules;
  @override
  int? initialPirSensitivity;
  @override
  String? firmwareVersion;
  @override
  SyncConfig? initialSyncConfig;

  SimulatedBleConnectionManager(this._bleService);

  @override
  String? get deviceId => _deviceId;

  @override
  Stream<LampConnectionState> get connectionState =>
      _connectionStateCtrl.stream;
  @override
  Stream<LedState> get ledStateStream => _ledStateCtrl.stream;
  @override
  Stream<SensorData> get sensorDataStream => _sensorDataCtrl.stream;
  @override
  Stream<List<Scene>> get sceneListStream => _sceneListCtrl.stream;
  @override
  Stream<List<Schedule>> get scheduleListStream => _scheduleListCtrl.stream;
  @override
  Stream<int> get otaStatusStream => _otaStatusCtrl.stream;

  @override
  Future<void> connect(String deviceId) async {
    _deviceId = deviceId;
    _connectionStateCtrl.add(LampConnectionState.connecting);

    await Future.delayed(const Duration(milliseconds: 300));

    initialLedState =
        const LedState(warm: 200, neutral: 100, cool: 50, master: 255);
    initialModeFlags = const ModeFlags();
    initialAutoConfig = const AutoConfig();
    initialFlameConfig = const FlameConfig();
    initialScenes = [
      const Scene(
          index: 0,
          name: 'Reading',
          warm: 255,
          neutral: 80,
          cool: 20,
          master: 200),
      const Scene(
          index: 1,
          name: 'Relax',
          warm: 180,
          neutral: 60,
          cool: 10,
          master: 140),
    ];
    initialSchedules = [];
    initialPirSensitivity = 24;
    firmwareVersion = 'SIM 1.0.0';

    _connectionStateCtrl.add(LampConnectionState.connected);
    _startSensorSimulation();
  }

  void _startSensorSimulation() {
    _sensorTimer?.cancel();
    _sensorTimer = Timer.periodic(const Duration(seconds: 5), (_) {
      _sensorDataCtrl.add(SensorData(
        lux: 10 + _random.nextInt(70),
        motion: _random.nextBool(),
      ));
    });
  }

  @override
  void disconnect() {
    _sensorTimer?.cancel();
    _connectionStateCtrl.add(LampConnectionState.disconnected);
    _deviceId = null;
  }

  @override
  void dispose() {
    _sensorTimer?.cancel();
    _connectionStateCtrl.close();
    _ledStateCtrl.close();
    _sensorDataCtrl.close();
    _sceneListCtrl.close();
    _scheduleListCtrl.close();
    _otaStatusCtrl.close();
  }
}
