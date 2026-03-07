import 'package:flutter_test/flutter_test.dart';
import 'package:smart_lamp/utils/circadian.dart';

void main() {
  group('CircadianCalculator', () {
    test('midnight returns warm only', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 0, 0));
      expect(r.warm, 255);
      expect(r.neutral, 0);
      expect(r.cool, 0);
    });

    test('6 AM boundary returns warm only', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 6, 0));
      expect(r.warm, 255);
      expect(r.neutral, 0);
      expect(r.cool, 0);
    });

    test('7 AM returns warm+neutral', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 7, 0));
      expect(r.warm, 230);
      expect(r.neutral, 80);
      expect(r.cool, 0);
    });

    test('noon returns peak cool', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 12, 0));
      expect(r.warm, 20);
      expect(r.neutral, 180);
      expect(r.cool, 255);
    });

    test('6 PM returns warm dominant', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 18, 0));
      expect(r.warm, 255);
      expect(r.neutral, 30);
      expect(r.cool, 0);
    });

    test('11 PM returns warm only', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 23, 0));
      expect(r.warm, 255);
      expect(r.neutral, 0);
      expect(r.cool, 0);
    });

    test('6:30 AM interpolates between 6AM and 7AM keyframes', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 6, 30));
      // Midpoint between (255,0,0) and (230,80,0)
      expect(r.warm, 243); // lerp(255, 230, 0.5) = 242.5 → 243
      expect(r.neutral, 40); // lerp(0, 80, 0.5) = 40
      expect(r.cool, 0);
    });

    test('23:59 returns warm only', () {
      final r = CircadianCalculator.calculate(DateTime(2026, 3, 6, 23, 59));
      expect(r.warm, 255);
      expect(r.neutral, 0);
      expect(r.cool, 0);
    });

    test('all values are clamped to 0-255', () {
      // Test several times to ensure no out-of-range values
      for (int h = 0; h < 24; h++) {
        for (int m = 0; m < 60; m += 15) {
          final r = CircadianCalculator.calculate(DateTime(2026, 1, 1, h, m));
          expect(r.warm, inInclusiveRange(0, 255), reason: 'warm at $h:$m');
          expect(r.neutral, inInclusiveRange(0, 255), reason: 'neutral at $h:$m');
          expect(r.cool, inInclusiveRange(0, 255), reason: 'cool at $h:$m');
        }
      }
    });
  });
}
