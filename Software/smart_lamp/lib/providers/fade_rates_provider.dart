import 'package:flutter_riverpod/flutter_riverpod.dart';

class FadeRates {
  final int fadeInSeconds;
  final int fadeOutSeconds;

  const FadeRates({this.fadeInSeconds = 3, this.fadeOutSeconds = 10});

  FadeRates copyWith({int? fadeInSeconds, int? fadeOutSeconds}) => FadeRates(
        fadeInSeconds: fadeInSeconds ?? this.fadeInSeconds,
        fadeOutSeconds: fadeOutSeconds ?? this.fadeOutSeconds,
      );
}

class FadeRatesNotifier extends StateNotifier<FadeRates> {
  FadeRatesNotifier() : super(const FadeRates());

  void setFadeIn(int seconds) =>
      state = state.copyWith(fadeInSeconds: seconds.clamp(0, 60));

  void setFadeOut(int seconds) =>
      state = state.copyWith(fadeOutSeconds: seconds.clamp(0, 60));
}

final fadeRatesProvider =
    StateNotifierProvider<FadeRatesNotifier, FadeRates>((ref) {
  return FadeRatesNotifier();
});
