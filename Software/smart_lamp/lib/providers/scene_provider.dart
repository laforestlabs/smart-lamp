import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../models/scene.dart';
import 'ble_provider.dart';

class SceneListNotifier extends StateNotifier<List<Scene>> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  StreamSubscription? _sub;

  SceneListNotifier(this._bleService, this._connManager) : super([]) {
    if (_connManager.initialScenes != null) {
      state = _connManager.initialScenes!;
    }
    _sub = _connManager.sceneListStream.listen((scenes) {
      state = scenes;
    });
  }

  Future<void> saveScene(Scene scene) async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.sceneWrite,
        BleCodec.encodeSceneWrite(scene),
      );
    } catch (_) {}
  }

  Future<void> deleteScene(int index) async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    // Write a scene with empty name to signal deletion
    final empty = Scene(index: index, name: '', warm: 0, neutral: 0, cool: 0, master: 0);
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.sceneWrite,
        BleCodec.encodeSceneWrite(empty),
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

final sceneListProvider =
    StateNotifierProvider<SceneListNotifier, List<Scene>>((ref) {
  return SceneListNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
