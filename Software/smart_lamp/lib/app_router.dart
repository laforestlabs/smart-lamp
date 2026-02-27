import 'package:go_router/go_router.dart';

import 'screens/auto_settings_screen.dart';
import 'screens/control_screen.dart';
import 'screens/flame_screen.dart';
import 'screens/group_sync_screen.dart';
import 'screens/home_screen.dart';
import 'screens/ota_screen.dart';
import 'screens/pairing_screen.dart';
import 'screens/scenes_screen.dart';
import 'screens/schedules_screen.dart';
import 'screens/settings_screen.dart';

final appRouter = GoRouter(
  initialLocation: '/',
  routes: [
    GoRoute(
      path: '/',
      builder: (context, state) => const HomeScreen(),
    ),
    GoRoute(
      path: '/pairing',
      builder: (context, state) => const PairingScreen(),
    ),
    GoRoute(
      path: '/settings',
      builder: (context, state) => const SettingsScreen(),
    ),
    GoRoute(
      path: '/control/:deviceId',
      builder: (context, state) => ControlScreen(
        deviceId: state.pathParameters['deviceId']!,
      ),
      routes: [
        GoRoute(
          path: 'auto',
          builder: (context, state) => const AutoSettingsScreen(),
        ),
        GoRoute(
          path: 'flame',
          builder: (context, state) => const FlameScreen(),
        ),
        GoRoute(
          path: 'scenes',
          builder: (context, state) => const ScenesScreen(),
        ),
        GoRoute(
          path: 'schedules',
          builder: (context, state) => const SchedulesScreen(),
        ),
        GoRoute(
          path: 'ota',
          builder: (context, state) => const OtaScreen(),
        ),
        GoRoute(
          path: 'group',
          builder: (context, state) => const GroupSyncScreen(),
        ),
      ],
    ),
  ],
);
