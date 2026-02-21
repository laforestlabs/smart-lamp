import 'package:flutter/material.dart';

class BrightnessSlider extends StatelessWidget {
  final int value;
  final ValueChanged<int> onChanged;

  const BrightnessSlider({
    super.key,
    required this.value,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Row(
        children: [
          const Icon(Icons.brightness_low, size: 20),
          Expanded(
            child: Slider(
              value: value / 255,
              onChanged: (v) => onChanged((v * 255).round()),
              label: '${(value / 2.55).round()}%',
            ),
          ),
          const Icon(Icons.brightness_high, size: 20),
          const SizedBox(width: 8),
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
