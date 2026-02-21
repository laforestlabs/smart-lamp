import 'dart:math';

import 'package:flutter/material.dart';

import '../models/flame_config.dart';

class FlameGridPreview extends StatelessWidget {
  final FlameConfig config;

  const FlameGridPreview({super.key, required this.config});

  // LED positions from spec §2.5
  static const _leds = [
    [1, 0], [2, 0], [3, 0],
    [0, 1], [1, 1], [2, 1], [3, 1], [4, 1],
    [0, 2], [1, 2], [2, 2], [3, 2], [4, 2],
    [0, 3], [1, 3], [2, 3], [3, 3], [4, 3],
    [0, 4], [1, 4], [2, 4], [3, 4], [4, 4],
    [0, 5], [1, 5], [2, 5], [3, 5], [4, 5],
    [1, 6], [2, 6], [3, 6],
  ];

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 5 / 7,
      child: CustomPaint(
        painter: _FlameGridPainter(config),
      ),
    );
  }
}

class _FlameGridPainter extends CustomPainter {
  final FlameConfig config;

  _FlameGridPainter(this.config);

  @override
  void paint(Canvas canvas, Size size) {
    final cellW = size.width / 5;
    final cellH = size.height / 7;
    final radius = 0.5 + config.radius / 255.0 * 3.5;
    final twoSigmaSq = 2.0 * radius * radius;
    // Approximate flame centre from config bias
    final fx = 2.0;
    final fy = config.biasY / 255.0 * 6.0;

    for (final led in FlameGridPreview._leds) {
      final col = led[0];
      final row = led[1];

      final dx = col - fx;
      final dy = row - fy;
      final d2 = dx * dx + dy * dy;
      final intensity = config.brightness / 255.0 * exp(-d2 / twoSigmaSq);

      final paint = Paint()
        ..color = Color.lerp(
          Colors.black,
          Colors.amber,
          intensity.clamp(0.0, 1.0),
        )!;

      final rect = RRect.fromRectAndRadius(
        Rect.fromLTWH(
          col * cellW + 2,
          row * cellH + 2,
          cellW - 4,
          cellH - 4,
        ),
        const Radius.circular(4),
      );
      canvas.drawRRect(rect, paint);
    }

    // Draw empty corner cells
    final emptyPaint = Paint()..color = Colors.white.withValues(alpha: 0.05);
    for (final pos in [
      [0, 0], [4, 0], [0, 6], [4, 6]
    ]) {
      final rect = RRect.fromRectAndRadius(
        Rect.fromLTWH(pos[0] * cellW + 2, pos[1] * cellH + 2, cellW - 4, cellH - 4),
        const Radius.circular(4),
      );
      canvas.drawRRect(rect, emptyPaint);
    }
  }

  @override
  bool shouldRepaint(covariant _FlameGridPainter oldDelegate) =>
      config != oldDelegate.config;
}
