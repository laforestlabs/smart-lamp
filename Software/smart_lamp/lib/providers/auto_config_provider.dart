import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../models/auto_config.dart';
import '../utils/debouncer.dart';
import 'ble_provider.dart';

class AutoConfigNotifier extends StateNotifier<AutoConfig> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  final Debouncer _debouncer = Debouncer(delay: const Duration(milliseconds: 200));

  AutoConfigNotifier(this._bleService, this._connManager) : super(const AutoConfig()) {
    if (_connManager.initialAutoConfig != null) {
      state = _connManager.initialAutoConfig!;
    }
  }

  void setTimeoutSeconds(int value) {
    state = state.copyWith(timeoutSeconds: value);
    _debouncedWrite();
  }

  void setLuxThreshold(int value) {
    state = state.copyWith(luxThreshold: value);
    _debouncedWrite();
  }

  void setDimPercent(int value) {
    state = state.copyWith(dimPercent: value);
    _debouncedWrite();
  }

  void setDimDurationSeconds(int value) {
    state = state.copyWith(dimDurationSeconds: value);
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
        BleUuids.autoConfig,
        BleCodec.encodeAutoConfig(state),
      );
    } catch (_) {}
  }

  @override
  void dispose() {
    _debouncer.dispose();
    super.dispose();
  }
}

final autoConfigProvider =
    StateNotifierProvider<AutoConfigNotifier, AutoConfig>((ref) {
  return AutoConfigNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
