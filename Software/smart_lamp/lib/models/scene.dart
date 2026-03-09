class Scene {
  final int index;
  final String name;
  final int warm;
  final int neutral;
  final int cool;
  final int master;
  final int modeFlags;
  final int fadeInSeconds;
  final int fadeOutSeconds;
  final int autoTimeoutSeconds;
  final int autoLuxThreshold;
  final int flameDriftX;
  final int flameDriftY;
  final int flameRestore;
  final int flameRadius;
  final int flameBiasY;
  final int flameFlickerDepth;
  final int flameFlickerSpeed;
  final int pirSensitivity;

  const Scene({
    required this.index,
    required this.name,
    this.warm = 0,
    this.neutral = 0,
    this.cool = 0,
    this.master = 128,
    this.modeFlags = 0,
    this.fadeInSeconds = 3,
    this.fadeOutSeconds = 10,
    this.autoTimeoutSeconds = 300,
    this.autoLuxThreshold = 50,
    this.flameDriftX = 128,
    this.flameDriftY = 102,
    this.flameRestore = 20,
    this.flameRadius = 128,
    this.flameBiasY = 128,
    this.flameFlickerDepth = 13,
    this.flameFlickerSpeed = 13,
    this.pirSensitivity = 24,
  });

  Scene copyWith({
    int? index,
    String? name,
    int? warm,
    int? neutral,
    int? cool,
    int? master,
    int? modeFlags,
    int? fadeInSeconds,
    int? fadeOutSeconds,
    int? autoTimeoutSeconds,
    int? autoLuxThreshold,
    int? flameDriftX,
    int? flameDriftY,
    int? flameRestore,
    int? flameRadius,
    int? flameBiasY,
    int? flameFlickerDepth,
    int? flameFlickerSpeed,
    int? pirSensitivity,
  }) {
    return Scene(
      index: index ?? this.index,
      name: name ?? this.name,
      warm: warm ?? this.warm,
      neutral: neutral ?? this.neutral,
      cool: cool ?? this.cool,
      master: master ?? this.master,
      modeFlags: modeFlags ?? this.modeFlags,
      fadeInSeconds: fadeInSeconds ?? this.fadeInSeconds,
      fadeOutSeconds: fadeOutSeconds ?? this.fadeOutSeconds,
      autoTimeoutSeconds: autoTimeoutSeconds ?? this.autoTimeoutSeconds,
      autoLuxThreshold: autoLuxThreshold ?? this.autoLuxThreshold,
      flameDriftX: flameDriftX ?? this.flameDriftX,
      flameDriftY: flameDriftY ?? this.flameDriftY,
      flameRestore: flameRestore ?? this.flameRestore,
      flameRadius: flameRadius ?? this.flameRadius,
      flameBiasY: flameBiasY ?? this.flameBiasY,
      flameFlickerDepth: flameFlickerDepth ?? this.flameFlickerDepth,
      flameFlickerSpeed: flameFlickerSpeed ?? this.flameFlickerSpeed,
      pirSensitivity: pirSensitivity ?? this.pirSensitivity,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is Scene &&
          index == other.index &&
          name == other.name &&
          modeFlags == other.modeFlags;

  @override
  int get hashCode => Object.hash(index, name, modeFlags);
}
