import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../models/flame_config.dart';
import '../utils/debouncer.dart';
import 'ble_provider.dart';

class FlameConfigNotifier extends StateNotifier<FlameConfig> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  final Debouncer _debouncer = Debouncer(delay: const Duration(milliseconds: 100));

  FlameConfigNotifier(this._bleService, this._connManager) : super(const FlameConfig()) {
    if (_connManager.initialFlameConfig != null) {
      state = _connManager.initialFlameConfig!;
    }
  }

  void setDrift(int value) {
    // Single slider maps proportionally to both X and Y
    final driftY = (value * 0.8).round();
    state = state.copyWith(driftX: value, driftY: driftY);
    _debouncedWrite();
  }

  void setRadius(int value) {
    state = state.copyWith(radius: value);
    _debouncedWrite();
  }

  void setFlickerDepth(int value) {
    state = state.copyWith(flickerDepth: value);
    _debouncedWrite();
  }

  void setFlickerSpeed(int value) {
    state = state.copyWith(flickerSpeed: value);
    _debouncedWrite();
  }

  void setBrightness(int value) {
    state = state.copyWith(brightness: value);
    _debouncedWrite();
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
        BleUuids.flameConfig,
        BleCodec.encodeFlameConfig(state),
      );
    } catch (_) {}
  }

  @override
  void dispose() {
    _debouncer.dispose();
    super.dispose();
  }
}

final flameConfigProvider =
    StateNotifierProvider<FlameConfigNotifier, FlameConfig>((ref) {
  return FlameConfigNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
