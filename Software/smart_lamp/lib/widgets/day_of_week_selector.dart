import 'package:flutter/material.dart';

class DayOfWeekSelector extends StatelessWidget {
  final int dayMask;
  final ValueChanged<int> onChanged;

  const DayOfWeekSelector({
    super.key,
    required this.dayMask,
    required this.onChanged,
  });

  static const _labels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];

  @override
  Widget build(BuildContext context) {
    return Wrap(
      spacing: 8,
      children: List.generate(7, (i) {
        final selected = (dayMask & (1 << i)) != 0;
        return FilterChip(
          label: Text(_labels[i]),
          selected: selected,
          onSelected: (val) {
            final newMask = val ? (dayMask | (1 << i)) : (dayMask & ~(1 << i));
            onChanged(newMask);
          },
        );
      }),
    );
  }
}
