import { ESPLoader, Transport } from "./vendor/esptool-js.js";

const $ = (id) => document.getElementById(id);
const logEl = $("log");
const busyBar = $("busy-bar");

const terminal = {
  clean() { logEl.textContent = ""; },
  write(s) { logEl.textContent += s; logEl.scrollTop = logEl.scrollHeight; },
  writeLine(s) { this.write(s + "\n"); },
};
const log = (s) => terminal.writeLine(s);
const setBusy = (on) => { busyBar.hidden = !on; };

let manifest = null;
let transport = null;
let loader = null;

if (!("serial" in navigator)) {
  $("unsupported").style.display = "";
  $("connect").disabled = true;
}

function selectedFw() {
  return document.querySelector('input[name="fw"]:checked').value;
}

function showMeta() {
  const meta = $("fw-meta");
  if (!manifest) return;
  const m = manifest[selectedFw()];
  meta.textContent = m
    ? `${m.file} · ${(m.size / 1024).toFixed(0)} KB · sha256 ${m.sha256.slice(0, 16)}…`
    : "firmware not found";
}

async function loadManifest() {
  try {
    const r = await fetch("firmware/manifest.json", { cache: "no-store" });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    manifest = await r.json();
    showMeta();
  } catch (e) {
    $("fw-meta").textContent = `could not load firmware list: ${e.message}`;
  }
}

document.querySelectorAll('input[name="fw"]').forEach((el) => el.addEventListener("change", showMeta));

async function disconnect() {
  try { if (transport) await transport.disconnect(); } catch (e) { /* already closed */ }
  transport = null;
  loader = null;
  $("controls").style.display = "none";
  $("connect").disabled = false;
  $("connect").textContent = "Connect";
  $("conn-status").textContent = "not connected";
}

$("connect").addEventListener("click", async () => {
  if (transport) { await disconnect(); return; }
  setBusy(true);
  $("monitor-hint").hidden = true;
  try {
    const port = await navigator.serial.requestPort();
    const baud = parseInt($("baud").value, 10);
    transport = new Transport(port, true);
    // romBaudrate == baudrate skips the port reopen after sync, which would reset this board
    // out of the bootloader (it has no auto-reset circuit to hold IO9 low).
    loader = new ESPLoader({ transport, baudrate: baud, romBaudrate: baud, terminal });
    terminal.clean();
    log("Connecting…");
    await loader.main("default_reset");

    const chipName = loader.chip.CHIP_NAME;
    let mac = "";
    try { mac = await loader.chip.readMac(loader); } catch (e) { /* cosmetic */ }
    if (chipName !== "ESP32-C3") {
      log(`Wrong chip: found ${chipName}, this firmware is for ESP32-C3. Not flashing.`);
      await disconnect();
      return;
    }
    log(`Connected: ${chipName}${mac ? " · MAC " + mac : ""}`);
    $("conn-status").textContent = `connected: ${chipName}${mac ? " · " + mac : ""}`;
    $("controls").style.display = "";
    $("connect").textContent = "Disconnect";
  } catch (e) {
    log(`Connect failed: ${e.message || e}`);
    log("Is the board in bootloader mode (hold BOOT, tap RST, release BOOT)?");
    await disconnect();
    $("monitor-hint").hidden = false;
  } finally {
    setBusy(false);
  }
});

$("flash").addEventListener("click", async () => {
  if (!loader || !manifest) return;
  const fw = selectedFw();
  const entry = manifest[fw];
  const status = $("fw-status");
  const progress = $("progress");
  $("flash").disabled = true;
  $("connect").disabled = true;
  setBusy(true);
  try {
    status.textContent = `downloading ${fw}…`;
    const r = await fetch(entry.file, { cache: "no-store" });
    if (!r.ok) throw new Error(`could not fetch ${entry.file}: HTTP ${r.status}`);
    const data = new Uint8Array(await r.arrayBuffer());
    if (data.length !== entry.size) throw new Error(`size mismatch: got ${data.length}, expected ${entry.size}`);

    log(`Flashing ${fw} (${data.length} bytes)…`);
    status.textContent = "flashing…";
    progress.hidden = false;
    progress.value = 0;
    await loader.writeFlash({
      fileArray: [{ data, address: 0x0 }],
      flashMode: "keep",
      flashFreq: "keep",
      flashSize: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (_i, written, total) => { progress.value = Math.round((written / total) * 100); },
    });
    progress.value = 100;
    log("Done. Tap RST (or replug USB) to run the new firmware.");
    status.textContent = `${fw} flashed — tap RST to run it`;
    await disconnect();
    $("conn-status").textContent = `${fw} flashed — tap RST (or replug USB) to run it`;
  } catch (e) {
    log(`Flash failed: ${e.message || e}`);
    status.textContent = "flash failed — see log";
    $("connect").disabled = false;
  } finally {
    $("flash").disabled = false;
    setBusy(false);
  }
});

loadManifest();
