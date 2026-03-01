import 'dart:convert';
import 'dart:typed_data';

import '../models/auto_config.dart';
import '../models/flame_config.dart';
import '../models/lamp_state.dart';
import '../models/scene.dart';
import '../models/schedule.dart';
import '../models/sensor_data.dart';
import '../models/sync_config.dart';

class BleCodec {
  BleCodec._();

  // ── LED State ──

  static LedState decodeLedState(List<int> bytes) {
    if (bytes.length < 4) return const LedState();
    return LedState(
      warm: bytes[0],
      neutral: bytes[1],
      cool: bytes[2],
      master: bytes[3],
    );
  }

  static List<int> encodeLedState(LedState state) {
    return [state.warm, state.neutral, state.cool, state.master];
  }

  // ── Mode Flags ──

  static ModeFlags decodeModeFlags(List<int> bytes) {
    if (bytes.isEmpty) return const ModeFlags();
    return ModeFlags.fromByte(bytes[0]);
  }

  static List<int> encodeModeFlags(ModeFlags flags) => [flags.toByte()];

  // ── Auto Config ──

  static AutoConfig decodeAutoConfig(List<int> bytes) {
    if (bytes.length < 7) return const AutoConfig();
    final bd = ByteData.sublistView(Uint8List.fromList(bytes));
    return AutoConfig(
      timeoutSeconds: bd.getUint16(0, Endian.little),
      luxThreshold: bd.getUint16(2, Endian.little),
      dimPercent: bytes[4],
      dimDurationSeconds: bd.getUint16(5, Endian.little),
    );
  }

  static List<int> encodeAutoConfig(AutoConfig cfg) {
    final bd = ByteData(7);
    bd.setUint16(0, cfg.timeoutSeconds, Endian.little);
    bd.setUint16(2, cfg.luxThreshold, Endian.little);
    bd.setUint8(4, cfg.dimPercent);
    bd.setUint16(5, cfg.dimDurationSeconds, Endian.little);
    return bd.buffer.asUint8List().toList();
  }

  // ── Scene Write ──

  static List<int> encodeSceneWrite(Scene scene) {
    final nameBytes = utf8.encode(scene.name);
    final len = nameBytes.length > 16 ? 16 : nameBytes.length;
    return [
      scene.index,
      len,
      ...nameBytes.take(len),
      scene.warm,
      scene.neutral,
      scene.cool,
      scene.master,
      scene.modeFlags,
    ];
  }

  // ── Scene List ──

  static List<Scene> decodeSceneList(List<int> bytes) {
    if (bytes.isEmpty) return [];
    final scenes = <Scene>[];
    int offset = 1; // skip count byte
    final count = bytes[0];
    for (int i = 0; i < count && offset < bytes.length; i++) {
      if (offset + 2 > bytes.length) break;
      final index = bytes[offset++];
      final nameLen = bytes[offset++];
      if (offset + nameLen + 4 > bytes.length) break;
      final name = utf8.decode(bytes.sublist(offset, offset + nameLen));
      offset += nameLen;
      final warm = bytes[offset++];
      final neutral = bytes[offset++];
      final cool = bytes[offset++];
      final master = bytes[offset++];
      final modeFlags = (offset < bytes.length) ? bytes[offset++] : 0;
      scenes.add(Scene(
        index: index,
        name: name,
        warm: warm,
        neutral: neutral,
        cool: cool,
        master: master,
        modeFlags: modeFlags,
      ));
    }
    return scenes;
  }

  // ── Schedule Write ──

  static List<int> encodeScheduleWrite(Schedule sched) {
    return [
      sched.index,
      sched.dayMask,
      sched.hour,
      sched.minute,
      sched.sceneIndex,
      sched.enabled ? 1 : 0,
    ];
  }

  // ── Schedule List ──

  static List<Schedule> decodeScheduleList(List<int> bytes) {
    if (bytes.isEmpty) return [];
    final schedules = <Schedule>[];
    int offset = 1; // skip count byte
    final count = bytes[0];
    for (int i = 0; i < count && offset + 6 <= bytes.length; i++) {
      schedules.add(Schedule(
        index: bytes[offset],
        dayMask: bytes[offset + 1],
        hour: bytes[offset + 2],
        minute: bytes[offset + 3],
        sceneIndex: bytes[offset + 4],
        enabled: bytes[offset + 5] != 0,
      ));
      offset += 6;
    }
    return schedules;
  }

  // ── Sensor Data ──

  static SensorData decodeSensorData(List<int> bytes) {
    if (bytes.length < 3) return const SensorData();
    final bd = ByteData.sublistView(Uint8List.fromList(bytes));
    return SensorData(
      lux: bd.getUint16(0, Endian.little),
      motion: bytes[2] != 0,
    );
  }

  // ── PIR Sensitivity ──

  static int decodePirSensitivity(List<int> bytes) {
    if (bytes.isEmpty) return 24; // default
    return bytes[0].clamp(0, 31);
  }

  static List<int> encodePirSensitivity(int level) => [level.clamp(0, 31)];

  // ── Flame Config ──

  static FlameConfig decodeFlameConfig(List<int> bytes) {
    if (bytes.length < 8) return const FlameConfig();
    return FlameConfig(
      driftX: bytes[0],
      driftY: bytes[1],
      restore: bytes[2],
      radius: bytes[3],
      biasY: bytes[4],
      flickerDepth: bytes[5],
      flickerSpeed: bytes[6],
      brightness: bytes[7],
    );
  }

  static List<int> encodeFlameConfig(FlameConfig cfg) {
    return [
      cfg.driftX,
      cfg.driftY,
      cfg.restore,
      cfg.radius,
      cfg.biasY,
      cfg.flickerDepth,
      cfg.flickerSpeed,
      cfg.brightness,
    ];
  }

  // ── Device Info ──

  static String decodeDeviceInfo(List<int> bytes) {
    if (bytes.isEmpty) return 'Unknown';
    return utf8.decode(bytes);
  }

  // ── Sync Config ──

  static SyncConfig decodeSyncConfig(List<int> bytes) {
    if (bytes.length < 7) return const SyncConfig();
    final groupId = bytes[0];
    final mac = bytes
        .sublist(1, 7)
        .map((b) => b.toRadixString(16).padLeft(2, '0').toUpperCase())
        .join(':');
    return SyncConfig(groupId: groupId, wifiMac: mac);
  }

  static List<int> encodeSyncGroup(int groupId) => [groupId.clamp(0, 255)];

  // ── Lamp Name ──

  static String decodeLampName(List<int> bytes) {
    if (bytes.isEmpty) return '';
    return utf8.decode(bytes);
  }

  static List<int> encodeLampName(String name) {
    final encoded = utf8.encode(name);
    return encoded.length > 32 ? encoded.sublist(0, 32) : encoded;
  }
}
