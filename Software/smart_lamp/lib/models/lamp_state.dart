enum LampMode { manual, auto, flame }

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
