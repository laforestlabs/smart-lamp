class Scene {
  final int index;
  final String name;
  final int warm;
  final int neutral;
  final int cool;
  final int master;

  const Scene({
    required this.index,
    required this.name,
    this.warm = 0,
    this.neutral = 0,
    this.cool = 0,
    this.master = 128,
  });

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is Scene && index == other.index && name == other.name;

  @override
  int get hashCode => Object.hash(index, name);
}
