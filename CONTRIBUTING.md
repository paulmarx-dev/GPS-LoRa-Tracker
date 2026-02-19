# Contributing to GPS LoRa Tracker

Thank you for your interest in contributing! Here's how you can help.

## Getting Started

1. **Fork** the repository
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/GPS-LoRa-Tracker.git
   cd GPS-LoRa-Tracker
   ```
3. **Create** a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Making Changes

### Code Style
- **C++**: Follow Arduino conventions
- **Naming**: `camelCase` for variables/functions, `CAPS_SNAKE_CASE` for constants
- **Comments**: Include clear comments, especially for complex logic
- **Commits**: Use descriptive messages (`"Add GPS fix validation"` not `"typo fix"`)

### Areas We Need Help With

#### High Priority
- [ ] Investigate device shutdown after ~2 hours (memory/watchdog issue)
- [ ] Add heap monitoring to OLED display
- [ ] Optimize WiFi/LoRa task switching
- [ ] Add comprehensive error handling

#### Medium Priority
- [ ] Web dashboard for live GPS visualization
- [ ] S3 integration for CSV export (AWS)
- [ ] MQTT alternative to HTTP uploads
- [ ] Battery life estimation algorithm

#### Nice to Have
- [x] Dark mode for OLED display
- [ ] Geofencing alerts
- [ ] Temperature sensor integration
- [ ] Support for other Heltec board variants

## Testing

Before submitting a PR, test locally:

```bash
# Clean build
pio run --target clean
pio run

# Upload to device
pio run --target upload

# Monitor
pio device monitor
```

Check:
- ✅ Firmware compiles without warnings
- ✅ Device joins LoRaWAN network
- ✅ WiFi connects and uploads
- ✅ OLED stats display correctly
- ✅ No memory creep (check serial output periodically)

## Submitting a Pull Request

1. **Push** your branch to GitHub
2. **Create** a Pull Request with:
   - Clear title: `"Fix: Device shutdown after 2hrs"`
   - Description of changes
   - Testing steps
   - Screenshots/logs if relevant

3. **Link** any related issues:
   ```markdown
   Fixes #123
   Related to #456
   ```

## Questions?

- **Technical Q**: Open an Issue with `[QUESTION]` tag
- **Ideas**: Start a Discussion
- **Bugs**: Create Issue with `[BUG]` tag + logs

## Code of Conduct

- Be respectful to all contributors
- No spam, self-promotion, or harassment
- Focus on constructive feedback
- Help others learn (we were all beginners once)

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

Thanks for helping make this project better! 🚀
