class SensorData {
  final int lux;
  final bool motion;

  const SensorData({this.lux = 0, this.motion = false});

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is SensorData && lux == other.lux && motion == other.motion;

  @override
  int get hashCode => Object.hash(lux, motion);
}
