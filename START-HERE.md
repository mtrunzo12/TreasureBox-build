# Paste this into Claude Code or Codex

I am a parent building this treasure chest with my child. Read README.md, KNOWN-ISSUES.md, CHECKLIST.md and firmware/DEVELOPER.md before suggesting changes. Explain this in small, plain-language steps.

First tell me what is included, what is missing, and which hardware decisions must be checked before I buy anything. Help me check the exported BOM against the exact components and connectors I can obtain. Do not invent an order-ready shopping cart.

Then walk me through one input or output at a time on a breadboard. Ask for my actual pin map and readings. Explain the piezo knock input, lid and tilt switches, keypad, servo, speaker and lights. Keep the bench pin map separate from the custom PCB pin map.

Before PCB ordering, walk me through opening both KiCad projects, reviewing the known USB-C issue, verifying footprints/connector orientation, running checks and inspecting Gerber/drill previews. Do not silently modify KiCad source as plain text; use KiCad or a validated CAD integration.

After assembly, help me compile and load the documented firmware, then test each function using CHECKLIST.md. Pause for measurements and photos instead of assuming the real hardware passed. Do not place orders, change device fuses or overwrite a programmed chip without explaining the exact action first.
