class AutoConfig {
  final int timeoutSeconds;
  final int luxThreshold;

  const AutoConfig({
    this.timeoutSeconds = 300,
    this.luxThreshold = 50,
  });

  AutoConfig copyWith({
    int? timeoutSeconds,
    int? luxThreshold,
  }) {
    return AutoConfig(
      timeoutSeconds: timeoutSeconds ?? this.timeoutSeconds,
      luxThreshold: luxThreshold ?? this.luxThreshold,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is AutoConfig &&
          timeoutSeconds == other.timeoutSeconds &&
          luxThreshold == other.luxThreshold;

  @override
  int get hashCode => Object.hash(timeoutSeconds, luxThreshold);
}
