import 'package:flutter/material.dart';

class ChannelSlider extends StatelessWidget {
  final String label;
  final int value;
  final Color color;
  final ValueChanged<int> onChanged;

  const ChannelSlider({
    super.key,
    required this.label,
    required this.value,
    required this.color,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 72,
            child: Text(label, style: Theme.of(context).textTheme.bodyMedium),
          ),
          Expanded(
            child: SliderTheme(
              data: SliderThemeData(activeTrackColor: color, thumbColor: color),
              child: Slider(
                value: value / 255,
                onChanged: (v) => onChanged((v * 255).round()),
                label: '${(value / 2.55).round()}%',
              ),
            ),
          ),
          SizedBox(
            width: 40,
            child: Text(
              '${(value / 2.55).round()}%',
              style: Theme.of(context).textTheme.bodySmall,
              textAlign: TextAlign.right,
            ),
          ),
        ],
      ),
    );
  }
}
