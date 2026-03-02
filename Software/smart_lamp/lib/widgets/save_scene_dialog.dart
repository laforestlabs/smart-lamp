import 'package:flutter/material.dart';

class SaveSceneDialog extends StatefulWidget {
  const SaveSceneDialog({super.key});

  @override
  State<SaveSceneDialog> createState() => _SaveSceneDialogState();
}

class _SaveSceneDialogState extends State<SaveSceneDialog> {
  final _controller = TextEditingController();
  int _fadeInSeconds = 3;
  int _fadeOutSeconds = 10;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Save Scene'),
      content: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            TextField(
              controller: _controller,
              autofocus: true,
              maxLength: 16,
              decoration: const InputDecoration(
                labelText: 'Scene name',
                hintText: 'e.g. Reading, Movie night',
              ),
              onSubmitted: (_) => _submit(),
            ),
            const SizedBox(height: 8),
            Text('Fade In: ${_fadeInSeconds}s',
                style: Theme.of(context).textTheme.bodySmall),
            Slider(
              value: _fadeInSeconds.toDouble(),
              min: 0,
              max: 60,
              divisions: 60,
              label: '${_fadeInSeconds}s',
              onChanged: (v) => setState(() => _fadeInSeconds = v.round()),
            ),
            const Text('Time to fade up when motion activates the lamp',
                style: TextStyle(fontSize: 11, color: Colors.grey)),
            const SizedBox(height: 8),
            Text('Fade Out: ${_fadeOutSeconds}s',
                style: Theme.of(context).textTheme.bodySmall),
            Slider(
              value: _fadeOutSeconds.toDouble(),
              min: 0,
              max: 60,
              divisions: 60,
              label: '${_fadeOutSeconds}s',
              onChanged: (v) => setState(() => _fadeOutSeconds = v.round()),
            ),
            const Text('Time to fade out after inactivity timeout',
                style: TextStyle(fontSize: 11, color: Colors.grey)),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: _submit,
          child: const Text('Save'),
        ),
      ],
    );
  }

  void _submit() {
    final name = _controller.text.trim();
    if (name.isNotEmpty) {
      Navigator.of(context).pop({
        'name': name,
        'fadeIn': _fadeInSeconds,
        'fadeOut': _fadeOutSeconds,
      });
    }
  }
}
