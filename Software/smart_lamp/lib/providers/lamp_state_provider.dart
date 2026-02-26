import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../models/lamp_state.dart';
import '../utils/debouncer.dart';
import 'ble_provider.dart';

class LampStateNotifier extends StateNotifier<LedState> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  final Debouncer _debouncer = Debouncer();
  StreamSubscription? _notifySub;

  LampStateNotifier(this._bleService, this._connManager) : super(const LedState()) {
    // Load initial state if available
    if (_connManager.initialLedState != null) {
      state = _connManager.initialLedState!;
    }
    // Listen for notifications
    _notifySub = _connManager.ledStateStream.listen((ledState) {
      state = ledState;
    });
  }

  void setWarm(int value) {
    state = state.copyWith(warm: value);
    _debouncedWrite();
  }

  void setNeutral(int value) {
    state = state.copyWith(neutral: value);
    _debouncedWrite();
  }

  void setCool(int value) {
    state = state.copyWith(cool: value);
    _debouncedWrite();
  }

  void setMaster(int value) {
    state = state.copyWith(master: value);
    _debouncedWrite();
  }

  void applyState(LedState newState) {
    state = newState;
    _writeNow();
  }

  void _debouncedWrite() {
    _debouncer.call(() => _writeNow());
  }

  Future<void> _writeNow() async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.ledState,
        BleCodec.encodeLedState(state),
      );
    } catch (_) {}
  }

  @override
  void dispose() {
    _notifySub?.cancel();
    _debouncer.dispose();
    super.dispose();
  }
}

final lampStateProvider =
    StateNotifierProvider<LampStateNotifier, LedState>((ref) {
  return LampStateNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});

class ModeFlagsNotifier extends StateNotifier<ModeFlags> {
  final BleService _bleService;
  final BleConnectionManager _connManager;

  ModeFlagsNotifier(this._bleService, this._connManager)
      : super(const ModeFlags()) {
    if (_connManager.initialModeFlags != null) {
      state = _connManager.initialModeFlags!;
    }
  }

  Future<void> setAuto(bool enabled) async {
    state = state.copyWith(autoEnabled: enabled);
    await _writeFlags();
  }

  Future<void> setFlame(bool enabled) async {
    state = state.copyWith(flameEnabled: enabled);
    await _writeFlags();
  }

  Future<void> _writeFlags() async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(
        deviceId,
        BleUuids.mode,
        BleCodec.encodeModeFlags(state),
      );
    } catch (_) {}
  }
}

final modeFlagsProvider =
    StateNotifierProvider<ModeFlagsNotifier, ModeFlags>((ref) {
  return ModeFlagsNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
