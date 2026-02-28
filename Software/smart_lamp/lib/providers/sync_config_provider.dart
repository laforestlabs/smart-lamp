import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../models/sync_config.dart';
import 'ble_provider.dart';

class SyncConfigNotifier extends StateNotifier<SyncConfig> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  StreamSubscription? _syncSub;

  SyncConfigNotifier(this._bleService, this._connManager)
      : super(_connManager.initialSyncConfig ?? const SyncConfig()) {
    _syncSub = _connManager.syncConfigStream.listen((config) {
      state = config;
    });
  }

  Future<void> setGroupId(int groupId) async {
    state = state.copyWith(groupId: groupId);
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.syncConfig,
        BleCodec.encodeSyncGroup(groupId),
      );
    } catch (_) {}
  }

  @override
  void dispose() {
    _syncSub?.cancel();
    super.dispose();
  }
}

final syncConfigProvider =
    StateNotifierProvider<SyncConfigNotifier, SyncConfig>((ref) {
  return SyncConfigNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
