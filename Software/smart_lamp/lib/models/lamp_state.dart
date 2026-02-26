class ModeFlags {
  final bool autoEnabled;
  final bool flameEnabled;

  const ModeFlags({this.autoEnabled = false, this.flameEnabled = false});

  ModeFlags copyWith({bool? autoEnabled, bool? flameEnabled}) {
    return ModeFlags(
      autoEnabled: autoEnabled ?? this.autoEnabled,
      flameEnabled: flameEnabled ?? this.flameEnabled,
    );
  }

  int toByte() => (autoEnabled ? 0x01 : 0) | (flameEnabled ? 0x02 : 0);

  factory ModeFlags.fromByte(int byte) => ModeFlags(
        autoEnabled: (byte & 0x01) != 0,
        flameEnabled: (byte & 0x02) != 0,
      );

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is ModeFlags &&
          autoEnabled == other.autoEnabled &&
          flameEnabled == other.flameEnabled;

  @override
  int get hashCode => Object.hash(autoEnabled, flameEnabled);
}

class LedState {
  final int warm;
  final int neutral;
  final int cool;
  final int master;

  const LedState({
    this.warm = 0,
    this.neutral = 0,
    this.cool = 0,
    this.master = 128,
  });

  LedState copyWith({int? warm, int? neutral, int? cool, int? master}) {
    return LedState(
      warm: warm ?? this.warm,
      neutral: neutral ?? this.neutral,
      cool: cool ?? this.cool,
      master: master ?? this.master,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is LedState &&
          warm == other.warm &&
          neutral == other.neutral &&
          cool == other.cool &&
          master == other.master;

  @override
  int get hashCode => Object.hash(warm, neutral, cool, master);
}
