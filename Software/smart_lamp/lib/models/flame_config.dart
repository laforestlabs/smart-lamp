class FlameConfig {
  final int driftX;
  final int driftY;
  final int restore;
  final int radius;
  final int biasY;
  final int flickerDepth;
  final int flickerSpeed;
  final int brightness;

  const FlameConfig({
    this.driftX = 128,
    this.driftY = 102,
    this.restore = 20,
    this.radius = 128,
    this.biasY = 128,
    this.flickerDepth = 13,
    this.flickerSpeed = 13,
    this.brightness = 255,
  });

  FlameConfig copyWith({
    int? driftX,
    int? driftY,
    int? restore,
    int? radius,
    int? biasY,
    int? flickerDepth,
    int? flickerSpeed,
    int? brightness,
  }) {
    return FlameConfig(
      driftX: driftX ?? this.driftX,
      driftY: driftY ?? this.driftY,
      restore: restore ?? this.restore,
      radius: radius ?? this.radius,
      biasY: biasY ?? this.biasY,
      flickerDepth: flickerDepth ?? this.flickerDepth,
      flickerSpeed: flickerSpeed ?? this.flickerSpeed,
      brightness: brightness ?? this.brightness,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is FlameConfig &&
          driftX == other.driftX &&
          driftY == other.driftY &&
          restore == other.restore &&
          radius == other.radius &&
          biasY == other.biasY &&
          flickerDepth == other.flickerDepth &&
          flickerSpeed == other.flickerSpeed &&
          brightness == other.brightness;

  @override
  int get hashCode => Object.hash(
      driftX, driftY, restore, radius, biasY, flickerDepth, flickerSpeed, brightness);
}
