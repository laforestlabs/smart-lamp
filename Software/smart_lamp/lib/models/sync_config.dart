class SyncConfig {
  final int groupId;
  final String wifiMac;

  const SyncConfig({this.groupId = 0, this.wifiMac = ''});

  SyncConfig copyWith({int? groupId, String? wifiMac}) {
    return SyncConfig(
      groupId: groupId ?? this.groupId,
      wifiMac: wifiMac ?? this.wifiMac,
    );
  }

  bool get isEnabled => groupId > 0;

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is SyncConfig &&
          groupId == other.groupId &&
          wifiMac == other.wifiMac;

  @override
  int get hashCode => Object.hash(groupId, wifiMac);
}
