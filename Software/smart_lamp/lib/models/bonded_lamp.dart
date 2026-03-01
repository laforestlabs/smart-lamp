class BondedLamp {
  final String deviceId;
  final String name;
  final DateTime? lastConnected;

  const BondedLamp({
    required this.deviceId,
    required this.name,
    this.lastConnected,
  });

  BondedLamp copyWith({String? name, DateTime? lastConnected}) => BondedLamp(
        deviceId: deviceId,
        name: name ?? this.name,
        lastConnected: lastConnected ?? this.lastConnected,
      );

  Map<String, dynamic> toJson() => {
        'deviceId': deviceId,
        'name': name,
        if (lastConnected != null)
          'lastConnected': lastConnected!.toIso8601String(),
      };

  factory BondedLamp.fromJson(Map<String, dynamic> json) => BondedLamp(
        deviceId: json['deviceId'] as String,
        name: json['name'] as String,
        lastConnected: json['lastConnected'] != null
            ? DateTime.parse(json['lastConnected'] as String)
            : null,
      );

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is BondedLamp && deviceId == other.deviceId;

  @override
  int get hashCode => deviceId.hashCode;
}
