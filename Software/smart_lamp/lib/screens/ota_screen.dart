import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/ble_provider.dart';
import '../providers/ota_provider.dart';

class OtaScreen extends ConsumerWidget {
  const OtaScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final otaState = ref.watch(otaProvider);
    final connManager = ref.read(connectionManagerProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Device')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Firmware info
          Card(
            child: ListTile(
              leading: const Icon(Icons.info_outline),
              title: const Text('Firmware Version'),
              subtitle: Text(connManager.firmwareVersion ?? 'Unknown'),
            ),
          ),
          const SizedBox(height: 24),

          // OTA Update
          Text('Firmware Update',
              style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),

          if (otaState.status == OtaStatus.idle) ...[
            FilledButton.icon(
              onPressed: () async {
                final result = await FilePicker.platform.pickFiles(
                  type: FileType.custom,
                  allowedExtensions: ['bin'],
                );
                if (result != null && result.files.single.path != null) {
                  ref
                      .read(otaProvider.notifier)
                      .startUpdate(File(result.files.single.path!));
                }
              },
              icon: const Icon(Icons.upload_file),
              label: const Text('Select Firmware File'),
            ),
          ],

          if (otaState.status == OtaStatus.transferring) ...[
            LinearProgressIndicator(value: otaState.progress),
            const SizedBox(height: 8),
            Text('Uploading: ${(otaState.progress * 100).toStringAsFixed(1)}%'),
            const SizedBox(height: 16),
            OutlinedButton(
              onPressed: () => ref.read(otaProvider.notifier).abort(),
              child: const Text('Cancel'),
            ),
          ],

          if (otaState.status == OtaStatus.completing) ...[
            const LinearProgressIndicator(),
            const SizedBox(height: 8),
            const Text('Finalising update...'),
          ],

          if (otaState.status == OtaStatus.done) ...[
            const Icon(Icons.check_circle, color: Colors.green, size: 48),
            const SizedBox(height: 8),
            const Text('Update complete! The lamp will restart.'),
            const SizedBox(height: 16),
            OutlinedButton(
              onPressed: () => ref.read(otaProvider.notifier).reset(),
              child: const Text('OK'),
            ),
          ],

          if (otaState.status == OtaStatus.error) ...[
            Icon(Icons.error, color: Theme.of(context).colorScheme.error, size: 48),
            const SizedBox(height: 8),
            Text('Update failed: ${otaState.errorMessage ?? "Unknown error"}'),
            const SizedBox(height: 16),
            OutlinedButton(
              onPressed: () => ref.read(otaProvider.notifier).reset(),
              child: const Text('Dismiss'),
            ),
          ],
        ],
      ),
    );
  }
}
