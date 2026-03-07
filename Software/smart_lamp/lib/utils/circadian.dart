/// Calculates warm/neutral/cool LED channel values based on time of day
/// for a natural circadian rhythm following a typical sun schedule.
class CircadianCalculator {
  CircadianCalculator._();

  /// Returns (warm, neutral, cool) for the given local time.
  ///
  /// Schedule:
  ///   midnight–6 AM: warm only (no blue light)
  ///   7 AM: warm + some neutral
  ///   8 AM: warm/neutral balanced
  ///   10 AM–2 PM: midday — neutral + cool dominant
  ///   5 PM: sunset approaching — warm returning
  ///   7 PM–midnight: warm only
  static ({int warm, int neutral, int cool}) calculate(DateTime time) {
    final fractionalHour = time.hour + time.minute / 60.0;

    // Keyframes: (hour, warm, neutral, cool)
    const keyframes = [
      (0.0, 255, 0, 0),
      (6.0, 255, 0, 0),
      (7.0, 230, 80, 0),
      (8.0, 180, 180, 0),
      (9.0, 100, 220, 60),
      (10.0, 40, 200, 200),
      (12.0, 20, 180, 255),
      (14.0, 40, 200, 200),
      (15.0, 100, 220, 60),
      (16.0, 180, 180, 0),
      (17.0, 220, 80, 0),
      (18.0, 255, 30, 0),
      (19.0, 255, 0, 0),
      (24.0, 255, 0, 0),
    ];

    for (int i = 0; i < keyframes.length - 1; i++) {
      final (h1, w1, n1, c1) = keyframes[i];
      final (h2, w2, n2, c2) = keyframes[i + 1];
      if (fractionalHour >= h1 && fractionalHour <= h2) {
        final t = (h2 == h1) ? 0.0 : (fractionalHour - h1) / (h2 - h1);
        return (
          warm: _lerp(w1, w2, t),
          neutral: _lerp(n1, n2, t),
          cool: _lerp(c1, c2, t),
        );
      }
    }
    return (warm: 255, neutral: 0, cool: 0);
  }

  static int _lerp(int a, int b, double t) =>
      (a + (b - a) * t).round().clamp(0, 255);
}
