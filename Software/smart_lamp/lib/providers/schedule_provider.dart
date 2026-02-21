import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../models/schedule.dart';
import 'ble_provider.dart';

class ScheduleListNotifier extends StateNotifier<List<Schedule>> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  StreamSubscription? _sub;

  ScheduleListNotifier(this._bleService, this._connManager) : super([]) {
    if (_connManager.initialSchedules != null) {
      state = _connManager.initialSchedules!;
    }
    _sub = _connManager.scheduleListStream.listen((schedules) {
      state = schedules;
    });
  }

  Future<void> saveSchedule(Schedule schedule) async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.scheduleWrite,
        BleCodec.encodeScheduleWrite(schedule),
      );
    } catch (_) {}
  }

  Future<void> deleteSchedule(int index) async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    final empty = Schedule(index: index, dayMask: 0, hour: 0, minute: 0, sceneIndex: 0, enabled: false);
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.scheduleWrite,
        BleCodec.encodeScheduleWrite(empty),
      );
      state = state.where((s) => s.index != index).toList();
    } catch (_) {}
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }
}

final scheduleListProvider =
    StateNotifierProvider<ScheduleListNotifier, List<Schedule>>((ref) {
  return ScheduleListNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
