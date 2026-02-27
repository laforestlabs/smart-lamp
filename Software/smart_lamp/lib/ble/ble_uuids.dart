import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class BleUuids {
  BleUuids._();

  static const _base = '0451-4000-B000-000000000000';

  static final serviceUuid = Uuid.parse('F000AA00-$_base');
  static final ledState = Uuid.parse('F000AA01-$_base');
  static final mode = Uuid.parse('F000AA02-$_base');
  static final autoConfig = Uuid.parse('F000AA03-$_base');
  static final sceneWrite = Uuid.parse('F000AA04-$_base');
  static final sceneList = Uuid.parse('F000AA05-$_base');
  static final scheduleWrite = Uuid.parse('F000AA06-$_base');
  static final scheduleList = Uuid.parse('F000AA07-$_base');
  static final sensorData = Uuid.parse('F000AA08-$_base');
  static final otaControl = Uuid.parse('F000AA09-$_base');
  static final otaData = Uuid.parse('F000AA0A-$_base');
  static final pirSensitivity = Uuid.parse('F000AA0B-$_base');
  static final flameConfig = Uuid.parse('F000AA0C-$_base');
  static final deviceInfo = Uuid.parse('F000AA0D-$_base');

  static QualifiedCharacteristic chr(String deviceId, Uuid charUuid) {
    return QualifiedCharacteristic(
      deviceId: deviceId,
      serviceId: serviceUuid,
      characteristicId: charUuid,
    );
  }
}
