import 'package:flutter/material.dart';

class SaveSceneDialog extends StatefulWidget {
  const SaveSceneDialog({super.key});

  @override
  State<SaveSceneDialog> createState() => _SaveSceneDialogState();
}

class _SaveSceneDialogState extends State<SaveSceneDialog> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Save Scene'),
      content: TextField(
        controller: _controller,
        autofocus: true,
        maxLength: 16,
        decoration: const InputDecoration(
          labelText: 'Scene name',
          hintText: 'e.g. Reading, Movie night',
        ),
        onSubmitted: (_) => _submit(),
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
      Navigator.of(context).pop(name);
    }
  }
}
