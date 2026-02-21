class BondedLamp {
  final String deviceId;
  final String name;

  const BondedLamp({required this.deviceId, required this.name});

  Map<String, dynamic> toJson() => {'deviceId': deviceId, 'name': name};

  factory BondedLamp.fromJson(Map<String, dynamic> json) => BondedLamp(
        deviceId: json['deviceId'] as String,
        name: json['name'] as String,
      );

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is BondedLamp && deviceId == other.deviceId;

  @override
  int get hashCode => deviceId.hashCode;
}
