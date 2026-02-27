import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/auto_config_provider.dart';
import '../providers/pir_sensitivity_provider.dart';
import '../providers/sensor_data_provider.dart';

class AutoSettingsScreen extends ConsumerWidget {
  const AutoSettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final config = ref.watch(autoConfigProvider);
    final pirSensitivity = ref.watch(pirSensitivityProvider);
    final sensorData = ref.watch(sensorDataProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Auto Mode Settings')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Live sensor readout
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: sensorData.when(
                data: (data) => Row(
                  mainAxisAlignment: MainAxisAlignment.spaceAround,
                  children: [
                    Column(
                      children: [
                        const Icon(Icons.light_mode),
                        const SizedBox(height: 4),
                        Text('${data.lux} lux',
                            style: Theme.of(context).textTheme.titleMedium),
                      ],
                    ),
                    Column(
                      children: [
                        Icon(data.motion
                            ? Icons.directions_walk
                            : Icons.person_off),
                        const SizedBox(height: 4),
                        Text(data.motion ? 'Motion' : 'No motion',
                            style: Theme.of(context).textTheme.titleMedium),
                      ],
                    ),
                  ],
                ),
                loading: () => const Center(child: Text('Waiting for sensor data...')),
                error: (_, _) => const Text('Sensor data unavailable'),
              ),
            ),
          ),
          const SizedBox(height: 24),

          // Motion sensitivity
          Text('Motion Sensitivity: $pirSensitivity/31',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: pirSensitivity.toDouble(),
            min: 0,
            max: 31,
            divisions: 31,
            label: '$pirSensitivity',
            onChanged: (v) =>
                ref.read(pirSensitivityProvider.notifier).setSensitivity(v.round()),
          ),
          const Text('Detection range \u2014 0 (closest) to 31 (farthest)',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
          const SizedBox(height: 16),

          // Lux threshold
          Text('Lux Threshold: ${config.luxThreshold}',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: config.luxThreshold.toDouble(),
            min: 0,
            max: 200,
            divisions: 40,
            label: '${config.luxThreshold} lux',
            onChanged: (v) =>
                ref.read(autoConfigProvider.notifier).setLuxThreshold(v.round()),
          ),
          const Text('Don\'t activate if room is brighter than this',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
          const SizedBox(height: 16),

          // Timeout
          Text('Timeout: ${config.timeoutSeconds}s',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: config.timeoutSeconds.toDouble(),
            min: 30,
            max: 600,
            divisions: 19,
            label: '${config.timeoutSeconds}s',
            onChanged: (v) =>
                ref.read(autoConfigProvider.notifier).setTimeoutSeconds(v.round()),
          ),
          const Text('Seconds of no motion before dimming',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
          const SizedBox(height: 16),

          // Dim level
          Text('Dim Level: ${config.dimPercent}%',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: config.dimPercent.toDouble(),
            min: 0,
            max: 100,
            divisions: 20,
            label: '${config.dimPercent}%',
            onChanged: (v) =>
                ref.read(autoConfigProvider.notifier).setDimPercent(v.round()),
          ),
          const Text('Brightness during warning phase',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
          const SizedBox(height: 16),

          // Dim duration
          Text('Dim Duration: ${config.dimDurationSeconds}s',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: config.dimDurationSeconds.toDouble(),
            min: 5,
            max: 120,
            divisions: 23,
            label: '${config.dimDurationSeconds}s',
            onChanged: (v) => ref
                .read(autoConfigProvider.notifier)
                .setDimDurationSeconds(v.round()),
          ),
          const Text('Time in dim state before turning off',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
        ],
      ),
    );
  }
}
