import 'package:flutter/material.dart';
import '../services/sysbotbase_client.dart';

class LumioseMapPainter extends CustomPainter {
  final double posX;
  final double posY;
  final List<Offset> trail;
  final bool shinyAlert;
  final Color accent;
  final double zoom;
  final bool followCam;

  LumioseMapPainter({
    required this.posX,
    required this.posY,
    required this.trail,
    required this.shinyAlert,
    required this.accent,
    required this.zoom,
    required this.followCam,
  });

  static Offset worldToNorm(double wx, double wzMap) {
    final nx = (0.50 + (wx - (-830.0)) / 980.0).clamp(0.0, 1.0);
    final ny = (0.50 - (wzMap - (-500.0)) / 980.0).clamp(0.0, 1.0);
    return Offset(nx, ny);
  }

  static Offset normToWorld(double nx, double ny) {
    final wx = -830.0 + (nx - 0.50) * 980.0;
    final wz = -500.0 - (ny - 0.50) * 980.0;
    return Offset(wx, wz);
  }

  @override
  void paint(Canvas canvas, Size size) {
    final playerNorm = worldToNorm(posX, posY);

    canvas.save();
    if (followCam) {
      final cx = size.width / 2;
      final cy = size.height / 2;
      canvas.translate(cx, cy);
      canvas.scale(zoom);
      canvas.translate(-playerNorm.dx * size.width, -playerNorm.dy * size.height);
    } else if (zoom != 1.0) {
      final cx = size.width / 2;
      final cy = size.height / 2;
      canvas.translate(cx, cy);
      canvas.scale(zoom);
      canvas.translate(-cx, -cy);
    }

    final center = Offset(size.width * 0.5, size.height * 0.5);
    final maxR = size.shortestSide * 0.46;

    // Grid & Lumiose rings
    final gridPaint = Paint()
      ..color = const Color(0xFF1A223B)
      ..strokeWidth = 1.0;
    for (int i = 1; i < 6; i++) {
      final gx = size.width * (i / 6);
      final gy = size.height * (i / 6);
      canvas.drawLine(Offset(gx, 0), Offset(gx, size.height), gridPaint);
      canvas.drawLine(Offset(0, gy), Offset(size.width, gy), gridPaint);
    }

    final ringPaint = Paint()
      ..color = const Color(0xFF283558)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.4;
    canvas.drawCircle(center, maxR, ringPaint);
    canvas.drawCircle(center, maxR * 0.65, ringPaint);
    canvas.drawCircle(center, maxR * 0.30, ringPaint);

    // 20 Wild Zones
    final zoneFill = Paint()..color = accent.withValues(alpha: 0.10);
    final zoneBorder = Paint()
      ..color = accent.withValues(alpha: 0.45)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0;

    for (final z in SysBotbaseClient.wildZones) {
      final p = Offset(z.nx * size.width, z.ny * size.height);
      final r = z.r * size.shortestSide * 0.65;
      canvas.drawCircle(p, r, zoneFill);
      canvas.drawCircle(p, r, zoneBorder);
      final tp = TextPainter(
        text: TextSpan(
          text: '${z.id}',
          style: TextStyle(
            color: accent.withValues(alpha: 0.85),
            fontSize: 9,
            fontWeight: FontWeight.bold,
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, p - Offset(tp.width / 2, tp.height / 2));
    }

    // Trail
    final trailPaint = Paint()..color = accent.withValues(alpha: 0.6);
    for (final pt in trail) {
      final n = worldToNorm(pt.dx, pt.dy);
      canvas.drawCircle(Offset(n.dx * size.width, n.dy * size.height), 2.2, trailPaint);
    }

    // Player radar ring & pin
    final pPx = Offset(playerNorm.dx * size.width, playerNorm.dy * size.height);
    final radarColor = shinyAlert ? const Color(0xFFFFD700) : accent;
    canvas.drawCircle(
      pPx,
      14,
      Paint()..color = radarColor.withValues(alpha: shinyAlert ? 0.30 : 0.18),
    );
    canvas.drawCircle(
      pPx,
      14,
      Paint()
        ..color = radarColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5,
    );
    canvas.drawCircle(pPx, 5.5, Paint()..color = const Color(0xFFFF1744));
    canvas.drawCircle(
      pPx,
      5.5,
      Paint()
        ..color = Colors.white
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5,
    );

    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant LumioseMapPainter old) => true;
}
