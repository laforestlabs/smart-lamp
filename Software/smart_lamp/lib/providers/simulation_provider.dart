import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'device_list_provider.dart';

const _key = 'simulation_mode';

class SimulationModeNotifier extends StateNotifier<bool> {
  final SharedPreferences _prefs;

  SimulationModeNotifier(this._prefs) : super(_prefs.getBool(_key) ?? false);

  Future<void> toggle() async {
    state = !state;
    await _prefs.setBool(_key, state);
  }
}

final simulationModeProvider =
    StateNotifierProvider<SimulationModeNotifier, bool>((ref) {
  return SimulationModeNotifier(ref.watch(sharedPreferencesProvider));
});
