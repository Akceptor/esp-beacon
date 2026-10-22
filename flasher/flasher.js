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

const CHIPS = { esp32c3: "ESP32-C3", esp8266: "ESP8266" };

function selectedFw() {
  return document.querySelector('input[name="fw"]:checked').value;
}

function selectedHw() {
  return document.querySelector('input[name="hw"]:checked').value;
}

function showHw() {
  const hw = selectedHw();
  document.querySelectorAll("[data-hw]").forEach((el) => { el.hidden = el.dataset.hw !== hw; });
  showMeta();
}

function showMeta() {
  const meta = $("fw-meta");
  if (!manifest) return;
  const m = manifest[selectedHw()]?.[selectedFw()];
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
document.querySelectorAll('input[name="hw"]').forEach((el) => el.addEventListener("change", showHw));

async function disconnect() {
  // A failed connect can leave the serial reader locked so disconnect() never settles; don't wait forever.
  try {
    if (transport) await Promise.race([transport.disconnect(), new Promise((r) => setTimeout(r, 2000))]);
  } catch (e) { /* already closed */ }
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
    const hw = selectedHw();
    transport = new Transport(port, true);
    // ESP32-C3: romBaudrate == baudrate skips the port reopen after sync, which would reset this
    // board out of the bootloader (no auto-reset circuit to hold IO9 low).
    // ESP8266: sync at 115200 and let esptool-js switch up after the stub loads; it falls back to
    // 115200 by itself if the switch fails.
    loader = new ESPLoader({ transport, baudrate: baud, romBaudrate: hw === "esp8266" ? 115200 : baud, terminal });
    terminal.clean();
    log("Connecting…");
    await loader.main("default_reset");

    const chipName = loader.chip.CHIP_NAME;
    let mac = "";
    try { mac = await loader.chip.readMac(loader); } catch (e) { /* cosmetic */ }
    // Flashing an image built for the other chip family would brick-loop the board until reflashed.
    // CHIP_NAME is "ESP8266" for ESP8285 too (same ROM, embedded flash).
    if (chipName !== CHIPS[hw]) {
      log(`Wrong chip: found ${chipName}, but you selected ${CHIPS[hw]}. Not flashing — pick the matching hardware in step 1.`);
      await disconnect();
      return;
    }
    log(`Connected: ${chipName}${mac ? " · MAC " + mac : ""}`);
    $("conn-status").textContent = `connected: ${chipName}${mac ? " · " + mac : ""}`;
    $("controls").style.display = "";
    $("connect").textContent = "Disconnect";
  } catch (e) {
    log(`Connect failed: ${e.message || e}`);
    log(selectedHw() === "esp8266"
      ? "Is the board in bootloader mode (hold FLASH/GPIO0, tap RST, release)?"
      : "Is the board in bootloader mode (hold BOOT, tap RST, release BOOT)?");
    $("monitor-hint").hidden = false;
    await disconnect();
  } finally {
    setBusy(false);
  }
});

$("flash").addEventListener("click", async () => {
  if (!loader || !manifest) return;
  const fw = selectedFw();
  const hw = loader.chip.CHIP_NAME === CHIPS.esp8266 ? "esp8266" : "esp32c3";
  if (hw !== selectedHw()) { log("Hardware selection changed after connecting. Disconnect and reconnect."); return; }
  const entry = manifest[hw]?.[fw];
  if (!entry) { log(`No ${hw} ${fw} image in manifest.`); return; }
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

    log(`Flashing ${CHIPS[hw]} ${fw} (${data.length} bytes)…`);
    status.textContent = "flashing…";
    progress.hidden = false;
    progress.value = 0;
    await loader.writeFlash({
      fileArray: [{ data, address: entry.offset }],
      // Image headers are rewritten only at the bootloader offset (0x0), so this applies to the
      // esp8266 image. DOUT works on every ESP8266/ESP8285 module; QIO/DIO headers hang 1 MB parts.
      flashMode: entry.flashMode || "keep",
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
