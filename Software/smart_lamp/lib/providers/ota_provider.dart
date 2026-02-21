import 'dart:async';
import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ble/ble_service.dart';
import '../ble/ble_uuids.dart';
import '../ble/ble_connection_manager.dart';
import 'ble_provider.dart';

enum OtaStatus { idle, transferring, completing, done, error }

class OtaState {
  final OtaStatus status;
  final double progress;
  final String? errorMessage;

  const OtaState({
    this.status = OtaStatus.idle,
    this.progress = 0,
    this.errorMessage,
  });

  OtaState copyWith({OtaStatus? status, double? progress, String? errorMessage}) {
    return OtaState(
      status: status ?? this.status,
      progress: progress ?? this.progress,
      errorMessage: errorMessage,
    );
  }
}

class OtaNotifier extends StateNotifier<OtaState> {
  final BleService _bleService;
  final BleConnectionManager _connManager;
  StreamSubscription? _statusSub;
  bool _aborted = false;

  OtaNotifier(this._bleService, this._connManager) : super(const OtaState());

  Future<void> startUpdate(File binFile) async {
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    _aborted = false;

    try {
      // Send OTA start command
      await _bleService.writeCharacteristic(deviceId, BleUuids.otaControl, [0x01]);
      state = state.copyWith(status: OtaStatus.transferring, progress: 0);

      final bytes = await binFile.readAsBytes();
      final totalBytes = bytes.length;
      const chunkSize = 509; // MTU 512 - 3 byte ATT header
      int offset = 0;

      while (offset < totalBytes && !_aborted) {
        final end = (offset + chunkSize > totalBytes) ? totalBytes : offset + chunkSize;
        final chunk = bytes.sublist(offset, end);

        await _bleService.writeWithoutResponse(deviceId, BleUuids.otaData, chunk);
        offset = end;
        state = state.copyWith(progress: offset / totalBytes);

        // Small delay to avoid overwhelming BLE link
        if (offset < totalBytes) {
          await Future.delayed(const Duration(milliseconds: 10));
        }
      }

      if (_aborted) return;

      // Send OTA end command
      state = state.copyWith(status: OtaStatus.completing);
      await _bleService.writeCharacteristic(deviceId, BleUuids.otaControl, [0x02]);
      state = state.copyWith(status: OtaStatus.done, progress: 1.0);
    } catch (e) {
      state = OtaState(status: OtaStatus.error, errorMessage: e.toString());
    }
  }

  Future<void> abort() async {
    _aborted = true;
    final deviceId = _connManager.deviceId;
    if (deviceId == null) return;
    try {
      await _bleService.writeCharacteristic(deviceId, BleUuids.otaControl, [0xFF]);
    } catch (_) {}
    state = const OtaState();
  }

  void reset() {
    state = const OtaState();
  }

  @override
  void dispose() {
    _statusSub?.cancel();
    super.dispose();
  }
}

final otaProvider = StateNotifierProvider<OtaNotifier, OtaState>((ref) {
  return OtaNotifier(
    ref.watch(bleServiceProvider),
    ref.watch(connectionManagerProvider),
  );
});
