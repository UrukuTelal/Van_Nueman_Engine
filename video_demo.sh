#!/bin/bash
# video_demo.sh - Record Van Nueman simulation demo
# Requires: OBS Studio, ffmpeg, or Similar screen recording tool

echo "Van Nueman Video Demo Script"
echo "================================"
echo ""
echo "This script outlines the steps to create a video demo of Van Nueman."
echo ""

# Configuration
OUTPUT_DIR="C:/Projects/Van_Nueman/docs/videos"
mkdir -p "$OUTPUT_DIR"

# Demo scenarios to record
SCENARIOS=("flocking" "harm_cascade" "distortion_outbreak")

echo "Demo Recording Steps:"
echo "1. Start OBS Studio or your preferred recording software"
echo "2. Configure recording:"
echo "   - Resolution: 1920x1080"
echo "   - FPS: 60"
echo "   - Format: MP4 (H.264)"
echo ""

for scenario in "${SCENARIOS[@]}"; do
    echo "=== Recording Scenario: $scenario ==="
    echo "   a. Run: van-nueman-game.exe --demo $scenario"
    echo "   b. Show Pillar HUD (press 'P' to toggle)"
    echo "   c. Record for 60 seconds"
    echo "   d. Stop recording"
    echo "   e. Save as: $OUTPUT_DIR/${scenario}.mp4"
    echo ""
done

echo "=== Post-Processing ==="
echo "1. Trim recordings to 45-60 seconds each"
echo "2. Add intro/outro with project logo"
echo "3. Overlay Pillar explanations during key moments"
echo "4. Upload to YouTube/GitHub releases"
echo ""

echo "Demo Checklist:"
echo "□ Scenario 1: Agent Flocking (green HUD bars)"
echo "□ Scenario 2: Harm Cascade (red Harm bar >0.7)"
echo "□ Scenario 3: Distortion Outbreak (red Distortion bar >0.5)"
echo "□ Show Pillar HUD throughout"
echo "□ Smooth transitions between scenarios"
echo "□ Background music (optional)"
echo ""
echo "Demo complete! Files saved to: $OUTPUT_DIR"
