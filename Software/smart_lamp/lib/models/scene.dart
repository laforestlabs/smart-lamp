class Scene {
  final int index;
  final String name;
  final int warm;
  final int neutral;
  final int cool;
  final int master;
  final int modeFlags;

  const Scene({
    required this.index,
    required this.name,
    this.warm = 0,
    this.neutral = 0,
    this.cool = 0,
    this.master = 128,
    this.modeFlags = 0,
  });

  Scene copyWith({
    int? index,
    String? name,
    int? warm,
    int? neutral,
    int? cool,
    int? master,
    int? modeFlags,
  }) {
    return Scene(
      index: index ?? this.index,
      name: name ?? this.name,
      warm: warm ?? this.warm,
      neutral: neutral ?? this.neutral,
      cool: cool ?? this.cool,
      master: master ?? this.master,
      modeFlags: modeFlags ?? this.modeFlags,
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
