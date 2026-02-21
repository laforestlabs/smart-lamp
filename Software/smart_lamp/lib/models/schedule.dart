class Schedule {
  final int index;
  final int dayMask; // bit 0 = Mon, bit 6 = Sun
  final int hour;
  final int minute;
  final int sceneIndex; // 0xFF = turn off
  final bool enabled;

  const Schedule({
    required this.index,
    this.dayMask = 0x7F,
    this.hour = 8,
    this.minute = 0,
    this.sceneIndex = 0,
    this.enabled = true,
  });

  Schedule copyWith({
    int? dayMask,
    int? hour,
    int? minute,
    int? sceneIndex,
    bool? enabled,
  }) {
    return Schedule(
      index: index,
      dayMask: dayMask ?? this.dayMask,
      hour: hour ?? this.hour,
      minute: minute ?? this.minute,
      sceneIndex: sceneIndex ?? this.sceneIndex,
      enabled: enabled ?? this.enabled,
    );
  }

  bool isActiveOn(int day) => (dayMask & (1 << day)) != 0;

  @override
  bool operator ==(Object other) =>
      identical(this, other) || other is Schedule && index == other.index;

  @override
  int get hashCode => index.hashCode;
}
