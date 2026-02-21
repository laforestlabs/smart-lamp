class AutoConfig {
  final int timeoutSeconds;
  final int luxThreshold;
  final int dimPercent;
  final int dimDurationSeconds;

  const AutoConfig({
    this.timeoutSeconds = 300,
    this.luxThreshold = 50,
    this.dimPercent = 30,
    this.dimDurationSeconds = 30,
  });

  AutoConfig copyWith({
    int? timeoutSeconds,
    int? luxThreshold,
    int? dimPercent,
    int? dimDurationSeconds,
  }) {
    return AutoConfig(
      timeoutSeconds: timeoutSeconds ?? this.timeoutSeconds,
      luxThreshold: luxThreshold ?? this.luxThreshold,
      dimPercent: dimPercent ?? this.dimPercent,
      dimDurationSeconds: dimDurationSeconds ?? this.dimDurationSeconds,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is AutoConfig &&
          timeoutSeconds == other.timeoutSeconds &&
          luxThreshold == other.luxThreshold &&
          dimPercent == other.dimPercent &&
          dimDurationSeconds == other.dimDurationSeconds;

  @override
  int get hashCode =>
      Object.hash(timeoutSeconds, luxThreshold, dimPercent, dimDurationSeconds);
}
