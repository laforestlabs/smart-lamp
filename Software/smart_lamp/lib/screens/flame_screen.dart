import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/flame_config_provider.dart';
import '../widgets/flame_grid_preview.dart';

class FlameScreen extends ConsumerWidget {
  const FlameScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final config = ref.watch(flameConfigProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Flame Mode')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Grid preview
          SizedBox(
            height: 280,
            child: Center(
              child: FlameGridPreview(config: config),
            ),
          ),
          const SizedBox(height: 24),

          // Intensity
          _buildSlider(
            context,
            label: 'Intensity',
            value: config.brightness,
            icon: Icons.brightness_high,
            onChanged: (v) =>
                ref.read(flameConfigProvider.notifier).setBrightness(v),
          ),
          const SizedBox(height: 8),

          // Drift (Calm ↔ Wild)
          _buildSlider(
            context,
            label: 'Calm \u2194 Wild',
            value: config.driftX,
            icon: Icons.air,
            onChanged: (v) =>
                ref.read(flameConfigProvider.notifier).setDrift(v),
          ),
          const SizedBox(height: 8),

          // Radius (Focused ↔ Diffuse)
          _buildSlider(
            context,
            label: 'Focused \u2194 Diffuse',
            value: config.radius,
            icon: Icons.blur_on,
            onChanged: (v) =>
                ref.read(flameConfigProvider.notifier).setRadius(v),
          ),
          const SizedBox(height: 8),

          // Flicker depth
          _buildSlider(
            context,
            label: 'Flicker',
            value: config.flickerDepth,
            icon: Icons.flash_on,
            onChanged: (v) =>
                ref.read(flameConfigProvider.notifier).setFlickerDepth(v),
          ),
          const SizedBox(height: 8),

          // Flicker speed
          _buildSlider(
            context,
            label: 'Slow \u2194 Fast',
            value: config.flickerSpeed,
            icon: Icons.speed,
            onChanged: (v) =>
                ref.read(flameConfigProvider.notifier).setFlickerSpeed(v),
          ),
        ],
      ),
    );
  }

  Widget _buildSlider(
    BuildContext context, {
    required String label,
    required int value,
    required IconData icon,
    required ValueChanged<int> onChanged,
  }) {
    return Row(
      children: [
        Icon(icon, size: 20),
        const SizedBox(width: 8),
        SizedBox(
          width: 120,
          child: Text(label, style: Theme.of(context).textTheme.bodyMedium),
        ),
        Expanded(
          child: Slider(
            value: value / 255,
            onChanged: (v) => onChanged((v * 255).round()),
          ),
        ),
        SizedBox(
          width: 36,
          child: Text('${(value / 2.55).round()}%',
              style: Theme.of(context).textTheme.bodySmall,
              textAlign: TextAlign.right),
        ),
      ],
    );
  }
}
