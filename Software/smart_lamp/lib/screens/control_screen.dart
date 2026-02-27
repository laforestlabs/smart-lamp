import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../ble/ble_connection_manager.dart';
import '../models/lamp_state.dart';
import '../models/scene.dart';
import '../providers/ble_provider.dart';
import '../providers/lamp_state_provider.dart';
import '../providers/scene_provider.dart';
import '../widgets/brightness_slider.dart';
import '../widgets/channel_slider.dart';
import '../widgets/mode_picker.dart';
import '../widgets/save_scene_dialog.dart';

class ControlScreen extends ConsumerWidget {
  final String deviceId;

  const ControlScreen({super.key, required this.deviceId});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final connState = ref.watch(connectionStateProvider);
    final flags = ref.watch(modeFlagsProvider);
    final ledState = ref.watch(lampStateProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Smart Lamp'),
        actions: [
          // Connection indicator
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: connState.when(
              data: (s) => Icon(
                s == LampConnectionState.connected
                    ? Icons.bluetooth_connected
                    : Icons.bluetooth_disabled,
                color: s == LampConnectionState.connected
                    ? Colors.green
                    : Colors.grey,
              ),
              loading: () => const SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(strokeWidth: 2),
              ),
              error: (_, _) => const Icon(Icons.bluetooth_disabled, color: Colors.red),
            ),
          ),
        ],
      ),
      body: ListView(
        children: [
          const SizedBox(height: 8),
          ModeToggles(
            flags: flags,
            onAutoChanged: (v) =>
                ref.read(modeFlagsProvider.notifier).setAuto(v),
            onFlameChanged: (v) =>
                ref.read(modeFlagsProvider.notifier).setFlame(v),
          ),
          const Divider(),
          Padding(
              padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
              child: SizedBox(
                width: double.infinity,
                height: 56,
                child: FilledButton.icon(
                  onPressed: () {
                    final notifier = ref.read(lampStateProvider.notifier);
                    if (ledState.master > 0) {
                      notifier.setMaster(0);
                    } else {
                      notifier.setMaster(128);
                    }
                  },
                  icon: Icon(
                    ledState.master > 0
                        ? Icons.lightbulb
                        : Icons.lightbulb_outline,
                    size: 28,
                  ),
                  label: Text(
                    ledState.master > 0 ? 'ON' : 'OFF',
                    style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                  style: FilledButton.styleFrom(
                    backgroundColor: ledState.master > 0
                        ? Colors.amber
                        : Colors.grey.shade800,
                    foregroundColor: ledState.master > 0
                        ? Colors.black
                        : Colors.white,
                  ),
                ),
              ),
            ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            child: Text('Colour Temperature',
                style: Theme.of(context).textTheme.titleSmall),
          ),
          ChannelSlider(
            label: 'Warm',
            value: ledState.warm,
            color: Colors.amber,
            onChanged: (v) => ref.read(lampStateProvider.notifier).setWarm(v),
          ),
          ChannelSlider(
            label: 'Neutral',
            value: ledState.neutral,
            color: Colors.white70,
            onChanged: (v) => ref.read(lampStateProvider.notifier).setNeutral(v),
          ),
          ChannelSlider(
            label: 'Cool',
            value: ledState.cool,
            color: Colors.lightBlue,
            onChanged: (v) => ref.read(lampStateProvider.notifier).setCool(v),
          ),
          const Divider(),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            child: Text('Brightness',
                style: Theme.of(context).textTheme.titleSmall),
          ),
          BrightnessSlider(
            value: ledState.master,
            onChanged: (v) => ref.read(lampStateProvider.notifier).setMaster(v),
          ),
          const SizedBox(height: 16),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: OutlinedButton.icon(
              onPressed: () async {
                final name = await showDialog<String>(
                  context: context,
                  builder: (_) => const SaveSceneDialog(),
                );
                if (name != null) {
                  final scenes = ref.read(sceneListProvider);
                  final index = scenes.isEmpty
                      ? 0
                      : scenes.map((s) => s.index).reduce((a, b) => a > b ? a : b) + 1;
                  final currentFlags = ref.read(modeFlagsProvider);
                  ref.read(sceneListProvider.notifier).saveScene(
                        Scene(
                          index: index,
                          name: name,
                          warm: ledState.warm,
                          neutral: ledState.neutral,
                          cool: ledState.cool,
                          master: ledState.master,
                          modeFlags: currentFlags.toByte(),
                        ),
                      );
                }
              },
              icon: const Icon(Icons.save),
              label: const Text('Save as Scene'),
            ),
          ),
          if (flags.autoEnabled)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Card(
                child: ListTile(
                  leading: const Icon(Icons.sensors),
                  title: const Text('Auto Mode Active'),
                  subtitle: const Text('Lamp responds to motion and ambient light'),
                  trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                  onTap: () => context.push('/control/$deviceId/auto'),
                ),
              ),
            ),
          if (flags.flameEnabled)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Card(
                child: ListTile(
                  leading: const Icon(Icons.local_fire_department),
                  title: const Text('Flame Effect Active'),
                  subtitle: const Text('Simulated candle flame animation'),
                  trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                  onTap: () => context.push('/control/$deviceId/flame'),
                ),
              ),
            ),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: 0,
        onDestinationSelected: (index) {
          switch (index) {
            case 1:
              context.push('/control/$deviceId/scenes');
              break;
            case 2:
              context.push('/control/$deviceId/schedules');
              break;
            case 3:
              context.push('/control/$deviceId/ota');
              break;
          }
        },
        destinations: const [
          NavigationDestination(icon: Icon(Icons.tune), label: 'Control'),
          NavigationDestination(icon: Icon(Icons.palette), label: 'Scenes'),
          NavigationDestination(icon: Icon(Icons.schedule), label: 'Schedules'),
          NavigationDestination(icon: Icon(Icons.info_outline), label: 'Device'),
        ],
      ),
    );
  }
}
