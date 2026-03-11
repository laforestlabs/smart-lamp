class AutoConfig {
  final int timeoutSeconds;
  final int luxThreshold;
  final int suppressMinutes;

  const AutoConfig({
    this.timeoutSeconds = 300,
    this.luxThreshold = 185,
    this.suppressMinutes = 60,
  });

  AutoConfig copyWith({
    int? timeoutSeconds,
    int? luxThreshold,
    int? suppressMinutes,
  }) {
    return AutoConfig(
      timeoutSeconds: timeoutSeconds ?? this.timeoutSeconds,
      luxThreshold: luxThreshold ?? this.luxThreshold,
      suppressMinutes: suppressMinutes ?? this.suppressMinutes,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is AutoConfig &&
          timeoutSeconds == other.timeoutSeconds &&
          luxThreshold == other.luxThreshold &&
          suppressMinutes == other.suppressMinutes;

  @override
  int get hashCode => Object.hash(timeoutSeconds, luxThreshold, suppressMinutes);
}
