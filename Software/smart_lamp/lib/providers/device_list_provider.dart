import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/bonded_lamp.dart';
import '../storage/local_storage.dart';

final sharedPreferencesProvider = Provider<SharedPreferences>((ref) {
  throw UnimplementedError('Must be overridden in main.dart');
});

final localStorageProvider = Provider<LocalStorage>((ref) {
  return LocalStorage(ref.watch(sharedPreferencesProvider));
});

class DeviceListNotifier extends StateNotifier<List<BondedLamp>> {
  final LocalStorage _storage;

  DeviceListNotifier(this._storage) : super(_storage.getBondedLamps());

  Future<void> addLamp(BondedLamp lamp) async {
    await _storage.addBondedLamp(lamp);
    state = _storage.getBondedLamps();
  }

  Future<void> removeLamp(String deviceId) async {
    await _storage.removeBondedLamp(deviceId);
    state = _storage.getBondedLamps();
  }

  Future<void> renameLamp(String deviceId, String newName) async {
    final lamp = state.firstWhere(
      (l) => l.deviceId == deviceId,
      orElse: () => BondedLamp(deviceId: deviceId, name: newName),
    );
    await _storage.addBondedLamp(BondedLamp(deviceId: deviceId, name: newName));
    state = _storage.getBondedLamps();
  }
}

final deviceListProvider =
    StateNotifierProvider<DeviceListNotifier, List<BondedLamp>>((ref) {
  return DeviceListNotifier(ref.watch(localStorageProvider));
});
