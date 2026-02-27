import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/lamp_state.dart';
import '../providers/lamp_state_provider.dart';
import '../providers/scene_provider.dart';
import '../widgets/scene_card.dart';

class ScenesScreen extends ConsumerWidget {
  const ScenesScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final scenes = ref.watch(sceneListProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Scenes')),
      body: scenes.isEmpty
          ? const Center(child: Text('No scenes saved yet'))
          : ListView.builder(
              itemCount: scenes.length,
              itemBuilder: (context, index) {
                final scene = scenes[index];
                return SceneCard(
                  scene: scene,
                  onApply: () {
                    ref.read(lampStateProvider.notifier).applyState(
                          LedState(
                            warm: scene.warm,
                            neutral: scene.neutral,
                            cool: scene.cool,
                            master: scene.master,
                          ),
                        );
                    final flags = ModeFlags.fromByte(scene.modeFlags);
                    ref.read(modeFlagsProvider.notifier).setAuto(flags.autoEnabled);
                    ref.read(modeFlagsProvider.notifier).setFlame(flags.flameEnabled);
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(content: Text('Applied: ${scene.name}')),
                    );
                  },
                  onDelete: () {
                    ref.read(sceneListProvider.notifier).deleteScene(scene.index);
                  },
                );
              },
            ),
    );
  }
}
