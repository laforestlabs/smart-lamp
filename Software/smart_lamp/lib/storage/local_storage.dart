import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

import '../models/bonded_lamp.dart';

class LocalStorage {
  static const _key = 'bonded_lamps';
  final SharedPreferences _prefs;

  LocalStorage(this._prefs);

  List<BondedLamp> getBondedLamps() {
    final raw = _prefs.getString(_key);
    if (raw == null) return [];
    final list = jsonDecode(raw) as List;
    return list
        .map((e) => BondedLamp.fromJson(e as Map<String, dynamic>))
        .toList();
  }

  Future<void> addBondedLamp(BondedLamp lamp) async {
    final lamps = getBondedLamps();
    lamps.removeWhere((l) => l.deviceId == lamp.deviceId);
    lamps.add(lamp);
    await _prefs.setString(_key, jsonEncode(lamps.map((l) => l.toJson()).toList()));
  }

  Future<void> removeBondedLamp(String deviceId) async {
    final lamps = getBondedLamps();
    lamps.removeWhere((l) => l.deviceId == deviceId);
    await _prefs.setString(_key, jsonEncode(lamps.map((l) => l.toJson()).toList()));
  }

  Future<void> updateLastConnected(String deviceId) async {
    final lamps = getBondedLamps();
    final idx = lamps.indexWhere((l) => l.deviceId == deviceId);
    if (idx < 0) return;
    lamps[idx] = lamps[idx].copyWith(lastConnected: DateTime.now());
    await _prefs.setString(_key, jsonEncode(lamps.map((l) => l.toJson()).toList()));
  }
}
