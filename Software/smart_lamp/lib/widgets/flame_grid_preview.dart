import 'dart:math';

import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';

import '../models/flame_config.dart';

class FlameGridPreview extends StatefulWidget {
  final FlameConfig config;

  const FlameGridPreview({super.key, required this.config});

  // LED positions from spec SS2.5
  static const leds = [
    [1, 0], [2, 0], [3, 0],
    [0, 1], [1, 1], [2, 1], [3, 1], [4, 1],
    [0, 2], [1, 2], [2, 2], [3, 2], [4, 2],
    [0, 3], [1, 3], [2, 3], [3, 3], [4, 3],
    [0, 4], [1, 4], [2, 4], [3, 4], [4, 4],
    [0, 5], [1, 5], [2, 5], [3, 5], [4, 5],
    [1, 6], [2, 6], [3, 6],
  ];

  @override
  State<FlameGridPreview> createState() => _FlameGridPreviewState();
}

class _FlameGridPreviewState extends State<FlameGridPreview>
    with SingleTickerProviderStateMixin {
  late Ticker _ticker;
  final _rng = Random();
  final _intensities = List<double>.filled(31, 0.0);

  // Flame centre (random walk state)
  double _fx = 2.0;
  double _fy = 3.0;

  // Flicker state
  double _flickerPhase = 0.0;
  double _flickerPhaseTarget = 0.0;
  int _flickerCounter = 0;
  int _flickerInterval = 30;

  Duration _lastTick = Duration.zero;

  @override
  void initState() {
    super.initState();
    _flickerPhaseTarget = _rng.nextDouble() * 2.0 * pi;
    _ticker = createTicker(_onTick);
    _ticker.start();
  }

  @override
  void dispose() {
    _ticker.dispose();
    super.dispose();
  }

  /// Box-Muller Gaussian random
  double _gaussian(double mean, double stddev) {
    final u1 = max(1e-10, _rng.nextDouble());
    final u2 = _rng.nextDouble();
    final z = sqrt(-2.0 * log(u1)) * cos(2.0 * pi * u2);
    return mean + stddev * z;
  }

  void _onTick(Duration elapsed) {
    // Run at ~30 fps
    if (elapsed - _lastTick < const Duration(milliseconds: 33)) return;
    _lastTick = elapsed;

    final cfg = widget.config;

    // Scale config values identically to firmware
    final driftX = cfg.driftX / 255.0 * 0.5;
    final driftY = cfg.driftY / 255.0 * 0.5;
    final kRestore = cfg.restore / 255.0 * 0.3;
    final sigma = 0.5 + cfg.radius / 255.0 * 3.5;
    final biasY = cfg.biasY / 255.0 * 6.0;
    final flDepth = cfg.flickerDepth / 255.0;
    final flSpeed = 1.0 + cfg.flickerSpeed / 255.0 * 9.0;
    final master = cfg.brightness / 255.0;

    // Random walk
    _fx += _gaussian(0, driftX) - kRestore * (_fx - 2.0);
    _fy += _gaussian(0, driftY) - kRestore * (_fy - biasY);
    _fx = _fx.clamp(1.0, 3.0);
    _fy = _fy.clamp(0.5, 5.5);

    // Flicker
    final t = elapsed.inMilliseconds / 1000.0;
    final flicker =
        1.0 - flDepth * (sin(t * flSpeed * 2.0 * pi + _flickerPhase)).abs();

    // Re-randomise flicker phase periodically
    _flickerCounter++;
    if (_flickerCounter >= _flickerInterval) {
      _flickerCounter = 0;
      _flickerPhase = _flickerPhaseTarget;
      _flickerPhaseTarget = _rng.nextDouble() * 2.0 * pi;
      _flickerInterval = 15 + _rng.nextInt(60);
    }
    _flickerPhase += (_flickerPhaseTarget - _flickerPhase) * 0.05;

    // Per-LED intensity (Gaussian falloff from flame centre)
    final twoSigmaSq = 2.0 * sigma * sigma;
    for (int i = 0; i < 31; i++) {
      final col = FlameGridPreview.leds[i][0].toDouble();
      final row = FlameGridPreview.leds[i][1].toDouble();
      final dx = col - _fx;
      final dy = row - _fy;
      final d2 = dx * dx + dy * dy;
      _intensities[i] = (master * exp(-d2 / twoSigmaSq) * flicker).clamp(0.0, 1.0);
    }

    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 5 / 7,
      child: CustomPaint(
        painter: _FlameGridPainter(_intensities),
      ),
    );
  }
}

class _FlameGridPainter extends CustomPainter {
  final List<double> intensities;

  _FlameGridPainter(this.intensities);

  @override
  void paint(Canvas canvas, Size size) {
    final cellW = size.width / 5;
    final cellH = size.height / 7;

    for (int i = 0; i < 31; i++) {
      final col = FlameGridPreview.leds[i][0];
      final row = FlameGridPreview.leds[i][1];

      final paint = Paint()
        ..color = Color.lerp(
          Colors.black,
          Colors.amber,
          intensities[i],
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
        Rect.fromLTWH(
            pos[0] * cellW + 2, pos[1] * cellH + 2, cellW - 4, cellH - 4),
        const Radius.circular(4),
      );
      canvas.drawRRect(rect, emptyPaint);
    }
  }

  @override
  bool shouldRepaint(covariant _FlameGridPainter oldDelegate) => true;
}
