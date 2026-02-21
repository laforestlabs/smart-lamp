import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/sensor_data.dart';
import 'ble_provider.dart';

final sensorDataProvider = StreamProvider<SensorData>((ref) {
  return ref.watch(connectionManagerProvider).sensorDataStream;
});
