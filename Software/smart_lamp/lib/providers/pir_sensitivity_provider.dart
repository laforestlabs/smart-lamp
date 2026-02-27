import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_codec.dart';
import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import '../utils/debouncer.dart';
import 'ble_provider.dart';

class PirSensitivityNotifier extends StateNotifier<int> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  final Debouncer _debouncer = Debouncer(delay: const Duration(milliseconds: 200));

  PirSensitivityNotifier(this._bleService, this._connManager)
      : super(_connManager.initialPirSensitivity ?? 24);

  void setSensitivity(int level) {
    state = level.clamp(0, 31);
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
        BleUuids.pirSensitivity,
        BleCodec.encodePirSensitivity(state),
      );
    } catch (_) {}
  }

  @override
  void dispose() {
    _debouncer.dispose();
    super.dispose();
  }
}

final pirSensitivityProvider =
    StateNotifierProvider<PirSensitivityNotifier, int>((ref) {
  return PirSensitivityNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
