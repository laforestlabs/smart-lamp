import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/auto_config_provider.dart';
import '../providers/fade_rates_provider.dart';
import '../providers/pir_sensitivity_provider.dart';
import '../providers/sensor_data_provider.dart';

class AutoSettingsScreen extends ConsumerWidget {
  const AutoSettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final config = ref.watch(autoConfigProvider);
    final pirSensitivity = ref.watch(pirSensitivityProvider);
    final fadeRates = ref.watch(fadeRatesProvider);
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
            min: 10,
            max: 600,
            divisions: 59,
            label: '${config.timeoutSeconds}s',
            onChanged: (v) =>
                ref.read(autoConfigProvider.notifier).setTimeoutSeconds(v.round()),
          ),
          const Text('Seconds of no motion before fade-out begins',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
          const SizedBox(height: 16),

          const Divider(),
          const SizedBox(height: 8),

          // Fade in
          Text('Fade In: ${fadeRates.fadeInSeconds}s',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: fadeRates.fadeInSeconds.toDouble(),
            min: 0,
            max: 60,
            divisions: 60,
            label: '${fadeRates.fadeInSeconds}s',
            onChanged: (v) =>
                ref.read(fadeRatesProvider.notifier).setFadeIn(v.round()),
          ),
          const Text('Time to fade up when motion activates the lamp',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
          const SizedBox(height: 16),

          // Fade out
          Text('Fade Out: ${fadeRates.fadeOutSeconds}s',
              style: Theme.of(context).textTheme.titleSmall),
          Slider(
            value: fadeRates.fadeOutSeconds.toDouble(),
            min: 0,
            max: 60,
            divisions: 60,
            label: '${fadeRates.fadeOutSeconds}s',
            onChanged: (v) =>
                ref.read(fadeRatesProvider.notifier).setFadeOut(v.round()),
          ),
          const Text('Time to fade out after inactivity timeout',
              style: TextStyle(fontSize: 12, color: Colors.grey)),
        ],
      ),
    );
  }
}
