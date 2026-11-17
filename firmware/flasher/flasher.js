// The Bleep web flasher: install the firmware over Web Serial with
// esptool-js, and a serial console for the remote's log. The firmware and
// its offsets come from firmware/manifest.json (build.sh writes it from the
// ESP-IDF build). Update writes the bootloader, the partition table, the OTA
// data and the app, and leaves the setup (LittleFS) and the secrets and
// Bluetooth bonds (NVS) alone; Clean install erases the whole flash first.
//
// Update first asks the firmware running on the remote to take the new app
// itself (esp/main/usb_update.c): it shows the progress on its screen, writes
// to its spare slot and switches only once the image checks out. A remote
// that doesn't answer (new, older firmware, not starting) or has another
// partition table gets the classic install from the chip's ROM loader.
import { ESPLoader, Transport } from "./vendor/esptool-js-0.7.0.js";

const ESPRESSIF_VID = 0x303a;   // the ESP32-S3's own USB (and its download mode)
const FLASH_BAUD = 921600;      // ignored by the USB-Serial/JTAG link, which runs at USB speed
const $ = (id) => document.getElementById(id);

let manifest = null;
let busy = false;

/* ---------- can this browser do it? ---------- */

function blocked(html) {
  $("no-serial-text").innerHTML = html;
  $("no-serial").classList.add("on");
  $("hdr-serial").innerHTML = '<span class="led bad">●</span> no web serial';
  $("install").disabled = true;
  $("con-connect").disabled = true;
  $("set-backup").disabled = $("set-restore").disabled = true;
}

if (!("serial" in navigator)) {
  if (!window.isSecureContext) {
    blocked("<b>This page has to be opened over https:// or as localhost</b> to reach USB devices. " +
            "Open it at <code>http://localhost:" + (location.port || "8081") + "/</code> instead.");
  } else {
    blocked("<b>This browser can't connect to USB devices.</b> Open this page in Chrome or Edge on a computer.");
  }
}

/* ---------- the firmware on offer ---------- */

function fmtDate(iso) {
  const d = new Date(iso + "T12:00:00");
  return isNaN(d) ? iso : d.toLocaleDateString(undefined, { day: "numeric", month: "long", year: "numeric" });
}

function selectedModel() {
  const id = document.querySelector('input[name="model"]:checked')?.value;
  return manifest?.models.find((m) => m.id === id) || null;
}

function showModel() {
  const m = selectedModel();
  if (!m) return;
  const pre = m.channel === "pre-release";
  $("fw-line").innerHTML = `firmware <b>${m.version}</b> · ${fmtDate(m.date)}` +
    (pre ? ' <span class="badge warn">pre-release</span> Not yet tried on a finished remote: expect rough edges.' : "");
  $("hdr-fw").innerHTML = `v${m.version}` + (pre ? ' <span class="warn-t">[pre-release]</span>' : "");
  $("footer-fw").textContent = `${m.name} firmware ${m.version}`;
}

async function loadManifest() {
  try {
    const r = await fetch("firmware/manifest.json", { cache: "no-store" });
    if (!r.ok) throw new Error(r.status);
    manifest = await r.json();
  } catch {
    $("models").innerHTML = '<p class="muted">The firmware list didn\'t load. Reload the page to try again.</p>';
    $("install").disabled = true;
    return;
  }
  const box = $("models");
  box.innerHTML = "";
  let first = true;
  for (const m of manifest.models) {
    const label = document.createElement("label");
    label.className = "choice";
    const ok = m.available;
    label.innerHTML =
      `<input type="radio" name="model" value="${m.id}" ${ok && first ? "checked" : ""} ${ok ? "" : "disabled"}>` +
      '<span class="dot" aria-hidden="true"></span>' +
      `<span><strong>${m.name}</strong> <span class="d">${m.detail}</span>` +
      `${ok ? "" : '<span class="badge">coming later</span>'}</span>`;
    box.appendChild(label);
    if (ok) first = false;
  }
  box.addEventListener("change", showModel);
  showModel();
}

/* ---------- install ---------- */

const log = (text, newline = true) => {
  const el = $("log");
  el.textContent += text + (newline ? "\n" : "");
  el.scrollTop = el.scrollHeight;
};

// Each new phase also goes in the trail above the bar, the one before it ticked off
function trail(text) {
  const box = $("trail");
  const last = box.lastElementChild;
  if (last && last.textContent === text) return;
  if (last) last.className = "done";
  const line = document.createElement("div");
  line.className = "now";
  line.textContent = text;
  box.appendChild(line);
}

function phase(text, pct) {
  trail(text);
  $("phase").textContent = text;
  $("pct").textContent = pct == null ? "" : `${Math.round(pct)}%`;
  $("bar").style.width = pct == null ? "100%" : `${pct}%`;
  $("bar").style.opacity = pct == null ? "0.35" : "1";
}

function result(kind, html) {
  const el = $("result");
  el.className = "result " + kind;
  el.innerHTML = html;
}

function friendly(err) {
  const msg = String(err && err.message ? err.message : err);
  if (err && err.name === "NetworkError" || /busy|already open|Failed to open/i.test(msg))
    return "The remote's connection is in use by another program or tab. Close it, then try again.";
  if (/timed? ?out|Failed to connect|sync/i.test(msg))
    return "The remote didn't answer. Unplug it, plug it back in, press a key to wake it, and try again. " +
           "If it still doesn't answer, use download mode (see “If it doesn't work” below).";
  if (/lost|disconnected|device has been lost/i.test(msg))
    return "The cable was unplugged or the connection dropped. Plug it back in and install again.";
  return "It stopped with: " + msg;
}

/* ---------- update through the running firmware ---------- */

const UPDATE_PROTOCOL = 1;

// The remote's lines ("@bleep ...") out of its log, which goes to the install log
function lineReader(reader) {
  const decoder = new TextDecoder();
  const queue = [], waiting = [];
  let text = "", closed = false;
  (async () => {
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        text += decoder.decode(value, { stream: true });
        let nl;
        while ((nl = text.indexOf("\n")) >= 0) {
          const line = text.slice(0, nl).replace(/\r$/, "");
          text = text.slice(nl + 1);
          const at = line.indexOf("@bleep ");
          if (at < 0) { if (line.trim()) log(line); continue; }
          const msg = line.slice(at + 7).trim();
          if (waiting.length) waiting.shift()(msg);
          else queue.push(msg);
        }
      }
    } catch { /* closed, or the remote restarted */ }
    closed = true;
    while (waiting.length) waiting.shift()(null);
  })();
  // the next line, or null after ms (or once the link is gone)
  return (ms) => {
    if (queue.length) return Promise.resolve(queue.shift());
    if (closed) return Promise.resolve(null);
    return new Promise((resolve) => {
      const done = (v) => { clearTimeout(timer); resolve(v); };
      const timer = setTimeout(() => {
        const i = waiting.indexOf(done);
        if (i >= 0) waiting.splice(i, 1);
        resolve(null);
      }, ms);
      waiting.push(done);
    });
  };
}

async function sha256hex(data) {
  const d = new Uint8Array(await crypto.subtle.digest("SHA-256", data));
  return Array.from(d, (b) => b.toString(16).padStart(2, "0")).join("");
}

// true: installed this way. false: not possible (said why in the log), the
// classic install follows. The port is closed again either way.
async function updateThroughFirmware(port, m) {
  const app = m.parts.find((p) => p.app);
  if (!app || !m.partition_table_sha) return false;
  await port.open({ baudRate: 115200 });
  const reader = port.readable.getReader(), writer = port.writable.getWriter();
  const next = lineReader(reader);
  const enc = new TextEncoder();
  const send = (line) => writer.write(enc.encode(line + "\n"));
  const expect = async (ms, ...words) => {
    for (;;) {
      const msg = await next(ms);
      if (msg == null) return null;
      if (words.some((w) => msg === w || msg.startsWith(w + " "))) return msg;
    }
  };
  try {
    phase("Asking the remote…");
    let hello = null;
    for (let i = 0; i < 2 && !hello; i++) {
      await send("\n@bleep hello");
      hello = await expect(1000, "hello");
    }
    if (!hello) {
      log("No answer from the firmware on the remote: installing the classic way.");
      return false;
    }
    const [, proto, model, version, slot, pt] = hello.split(" ");
    let why = null;
    if (Number(proto) !== UPDATE_PROTOCOL) why = "it speaks another version of the update link";
    else if (model !== m.id) why = `it's a ${model}, this is firmware for a ${m.id}`;
    else if (Number(slot) < app.size) why = "the new firmware doesn't fit its update slot";
    else if (pt !== m.partition_table_sha) why = "this firmware changes how the flash is laid out";
    if (why) {
      log(`The remote runs firmware ${version}, but ${why}: installing the classic way.`);
      return false;
    }
    trail(`Connected · Bleep firmware ${version}, updating through it`);

    phase("Getting the firmware…");
    const r = await fetch(app.path, { cache: "no-store" });
    if (!r.ok) throw new Error(`couldn't load ${app.path} (${r.status})`);
    const data = new Uint8Array(await r.arrayBuffer());
    const sha = await sha256hex(data);

    await send(`@bleep update ${data.length} ${sha}`);
    const ready = await expect(10000, "ready", "fail");
    if (!ready || ready.startsWith("fail")) {
      log(`The remote didn't take the update (${ready || "no answer"}): installing the classic way.`);
      return false;
    }
    const [, chunk, windowBytes] = ready.split(" ").map(Number);
    phase("Installing… (watch the remote's screen)", 0);
    let sent = 0, acked = 0;
    while (acked < data.length) {
      while (sent < data.length && sent - acked < windowBytes) {
        const n = Math.min(chunk, data.length - sent);
        await writer.write(data.subarray(sent, sent + n));
        sent += n;
      }
      const msg = await expect(10000, "ack", "fail");
      if (!msg || msg.startsWith("fail")) {
        log(`The update stopped (${msg || "no answer"}); the remote keeps its firmware. Installing the classic way.`);
        return false;
      }
      acked = Number(msg.split(" ")[1]);
      phase("Installing… (watch the remote's screen)", (acked / data.length) * 100);
    }
    phase("Checking it…", 100);
    const end = await expect(15000, "done", "fail");
    if (end !== "done") {
      log(`The remote didn't accept it (${end || "no answer"}); it keeps its firmware. Installing the classic way.`);
      return false;
    }
    phase("Restarting the remote…", 100);
    return true;
  } finally {
    try { await reader.cancel(); } catch {}
    try { reader.releaseLock(); } catch {}
    try { writer.releaseLock(); } catch {}
    try { await port.close(); } catch {}
  }
}

async function install() {
  if (busy) return;
  const m = selectedModel();
  if (!m) return;
  const clean = document.querySelector('input[name="mode"]:checked').value === "clean";
  if (clean && !confirm("Clean install erases everything on the remote: devices, activities, routines, Wi-Fi and paired TVs. " +
              "Back up your settings first (below) to restore them after. Continue?"))
    return;
  await consoleStop();

  let port;
  try {
    port = await navigator.serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF_VID }] });
  } catch {
    return;   // closed the list without picking one
  }

  busy = true;
  $("install").disabled = true;
  $("con-connect").disabled = true;
  $("log").textContent = "";
  $("trail").textContent = "";
  $("progress").classList.add("on");
  result("", "");
  let transport = null;
  try {
    if (!clean && await updateThroughFirmware(port, m)) {
      result("ok", `<b>Done.</b> Your ${m.name} checked the new firmware, ${m.version}, and is restarting with it. ` +
        "Your setup is as you left it.");
      phase("Done", 100);
      $("trail").lastElementChild.className = "done";
      return;
    }
    phase("Connecting…");
    transport = new Transport(port, false);
    const loader = new ESPLoader({
      transport,
      baudrate: FLASH_BAUD,
      terminal: { clean() { $("log").textContent = ""; }, writeLine: (d) => log(d), write: (d) => log(d, false) },
    });
    const chip = await loader.main();
    log(`Connected: ${chip}`);
    trail(`Connected · ${chip}`);
    if (!/ESP32-S3/i.test(chip)) throw new Error(`this device is an ${chip}, not a ${m.name}`);

    phase("Getting the firmware…");
    const files = [];
    for (const p of m.parts) {
      const r = await fetch(p.path, { cache: "no-store" });
      if (!r.ok) throw new Error(`couldn't load ${p.path} (${r.status})`);
      files.push({ data: new Uint8Array(await r.arrayBuffer()), address: p.offset });
    }
    const total = files.reduce((n, f) => n + f.data.length, 0);
    const before = files.map((_, i) => files.slice(0, i).reduce((n, f) => n + f.data.length, 0));

    if (clean) {
      phase("Erasing everything… (about half a minute)");
      await loader.eraseFlash();
    }

    phase("Installing…", 0);
    await loader.writeFlash({
      fileArray: files,
      flashMode: m.flash.mode,
      flashFreq: m.flash.freq,
      flashSize: m.flash.size,
      eraseAll: false,
      compress: true,
      reportProgress: (i, written, size) => {
        const done = before[i] + (size ? (written / size) * files[i].data.length : 0);
        phase("Installing…", (done / total) * 100);
      },
    });

    phase("Restarting the remote…", 100);
    await loader.after("hard_reset");
    result("ok", `<b>Done.</b> Your ${m.name} is restarting with firmware ${m.version}.` +
      (clean ? " It starts as new: set it up from the first screen." : " Your setup is as you left it."));
    phase("Done", 100);
    $("trail").lastElementChild.className = "done";
  } catch (err) {
    log(String(err && err.stack ? err.stack : err));
    const last = $("trail").lastElementChild;
    if (last) last.className = "fail";
    $("progress").classList.remove("on");
    result("err", "<b>The install didn't finish.</b> " + friendly(err));
    $("log-box").open = true;
  } finally {
    if (transport) try { await transport.disconnect(); } catch { /* already gone */ }
    busy = false;
    $("install").disabled = false;
    $("con-connect").disabled = false;
  }
}

$("install").addEventListener("click", install);

/* ---------- settings backup and restore (esp/main/usb_update.c) ---------- */

// The remote's firmware, ready for @bleep commands; null if it doesn't answer
async function openLink(port) {
  await port.open({ baudRate: 115200 });
  const reader = port.readable.getReader(), writer = port.writable.getWriter();
  const next = lineReader(reader);
  const enc = new TextEncoder();
  const link = {
    writer,
    send: (line) => writer.write(enc.encode(line + "\n")),
    expect: async (ms, ...words) => {
      for (;;) {
        const msg = await next(ms);
        if (msg == null) return null;
        if (words.some((w) => msg === w || msg.startsWith(w + " "))) return msg;
      }
    },
    close: async () => {
      try { await reader.cancel(); } catch {}
      try { reader.releaseLock(); } catch {}
      try { writer.releaseLock(); } catch {}
      try { await port.close(); } catch {}
    },
  };
  for (let i = 0; i < 2 && !link.hello; i++) {
    await link.send("\n@bleep hello");
    link.hello = await link.expect(1000, "hello");
  }
  if (!link.hello) {
    await link.close();
    throw new Error("The remote didn't answer. Press a key to wake it, check the cable, and try again.");
  }
  link.version = link.hello.split(" ")[3];
  return link;
}

const OLD_FIRMWARE = (v) =>
  `The remote runs firmware ${v}, which can't do this yet. Install the latest firmware above (update keeps your setup), then try again.`;

function setResult(kind, html) {
  const el = $("set-result");
  el.className = "result " + kind;
  el.innerHTML = html;
}

async function settingsJob(label, job) {
  if (busy) return;
  await consoleStop();
  let port;
  try {
    port = await navigator.serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF_VID }] });
  } catch {
    return;
  }
  busy = true;
  for (const id of ["install", "con-connect", "set-backup", "set-restore"]) $(id).disabled = true;
  setResult("", "");
  $("set-status").textContent = label;
  let link = null;
  try {
    link = await openLink(port);
    await job(link);
  } catch (err) {
    setResult("err", "<b>It didn't work.</b> " + (err && err.plain ? err.message : friendly(err)));
  } finally {
    if (link) await link.close();
    $("set-status").textContent = "";
    busy = false;
    for (const id of ["install", "con-connect", "set-backup", "set-restore"]) $(id).disabled = false;
  }
}

const plain = (msg) => Object.assign(new Error(msg), { plain: true });

async function backup(link) {
  await link.send("@bleep backup");
  const head = await link.expect(15000, "file", "fail");
  if (!head) throw plain(OLD_FIRMWARE(link.version));
  if (head.startsWith("fail")) throw plain("The remote said: " + head.slice(5));
  const [, size, sha] = head.split(" ");
  let b64 = "";
  for (;;) {
    const msg = await link.expect(5000, "b", "end", "fail");
    if (!msg || msg.startsWith("fail")) throw plain("The backup stopped halfway. Try again.");
    if (msg === "end") break;
    b64 += msg.slice(2);
    $("set-status").textContent = `Backing up… ${Math.min(99, Math.round((b64.length * 0.75 / size) * 100))}%`;
  }
  const bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
  if (bytes.length !== Number(size) || (await sha256hex(bytes)) !== sha)
    throw plain("The backup arrived damaged. Try again.");
  return JSON.parse(new TextDecoder().decode(bytes));
}

/* ---- the file: encrypted with a password (PBKDF2-SHA256 -> AES-256-GCM, WebCrypto),
 * or without the secrets. The remote only ever sees the plain backup. ---- */

const KDF_ITERATIONS = 600000;
function b64of(u8) {
  let s = "";
  for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
  return btoa(s);
}
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));

async function keyFrom(password, salt, iterations) {
  const base = await crypto.subtle.importKey("raw", new TextEncoder().encode(password), "PBKDF2", false, ["deriveKey"]);
  return crypto.subtle.deriveKey({ name: "PBKDF2", hash: "SHA-256", salt, iterations }, base,
                                 { name: "AES-GCM", length: 256 }, false, ["encrypt", "decrypt"]);
}

async function encryptBackup(backup, password) {
  const salt = crypto.getRandomValues(new Uint8Array(16)), iv = crypto.getRandomValues(new Uint8Array(12));
  const key = await keyFrom(password, salt, KDF_ITERATIONS);
  const data = new Uint8Array(await crypto.subtle.encrypt({ name: "AES-GCM", iv }, key,
                                                          new TextEncoder().encode(JSON.stringify(backup))));
  return { bleep_backup_encrypted: 1, firmware: backup.firmware, cipher: "AES-256-GCM", kdf: "PBKDF2-SHA256",
           iterations: KDF_ITERATIONS, salt: b64of(salt), iv: b64of(iv), data: b64of(data) };
}

// the plain backup, or null for a wrong password (GCM's tag doesn't match)
async function decryptBackup(file, password) {
  const key = await keyFrom(password, unb64(file.salt), file.iterations);
  try {
    const plainBytes = await crypto.subtle.decrypt({ name: "AES-GCM", iv: unb64(file.iv) }, key, unb64(file.data));
    return JSON.parse(new TextDecoder().decode(plainBytes));
  } catch {
    return null;
  }
}

function saveFile(obj, suffix) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([JSON.stringify(obj, null, 1) + "\n"], { type: "application/json" }));
  a.download = `bleep-settings-${new Date().toISOString().slice(0, 10)}${suffix}.json`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  return a.download;
}

/* The password dialog. Resolves to the password, "" for "without secrets", null for cancel.
 * check(pw) may return an error to show and keep it open (a wrong password). */
function askPassword({ title, text, repeat, skip, check }) {
  const d = $("pw");
  $("pw-title").textContent = title;
  $("pw-text").textContent = text;
  $("pw-2-row").hidden = !repeat;
  $("pw-skip").hidden = !skip;
  $("pw-1").value = $("pw-2").value = "";
  $("pw-1").autocomplete = repeat ? "new-password" : "current-password";
  $("pw-err").textContent = "";
  d.showModal();
  $("pw-1").focus();
  return new Promise((resolve) => {
    const done = (v) => {
      $("pw-form").onsubmit = $("pw-cancel").onclick = $("pw-skip").onclick = d.oncancel = null;
      d.close();
      resolve(v);
    };
    $("pw-cancel").onclick = () => done(null);
    d.oncancel = (e) => { e.preventDefault(); done(null); };
    $("pw-skip").onclick = () => done("");
    $("pw-form").onsubmit = async (e) => {
      e.preventDefault();
      const pw = $("pw-1").value;
      if (!pw) return ($("pw-err").textContent = "Type a password.");
      if (repeat && pw.length < 8) return ($("pw-err").textContent = "At least 8 characters.");
      if (repeat && pw !== $("pw-2").value) return ($("pw-err").textContent = "The two don't match.");
      if (check) {
        $("pw-ok").disabled = true;
        $("pw-err").textContent = "Checking…";
        const err = await check(pw);
        $("pw-ok").disabled = false;
        if (err) return ($("pw-err").textContent = err);
      }
      done(pw);
    };
  });
}

async function restore(link, data) {
  const sha = await sha256hex(data);
  await link.send(`@bleep restore ${data.length} ${sha}`);
  const ready = await link.expect(5000, "ready", "fail");
  if (!ready) throw plain(OLD_FIRMWARE(link.version));
  if (ready.startsWith("fail")) throw plain("The remote said: " + ready.slice(5));
  const [, chunk, windowBytes] = ready.split(" ").map(Number);
  let sent = 0, acked = 0;
  while (acked < data.length) {
    while (sent < data.length && sent - acked < windowBytes) {
      const n = Math.min(chunk, data.length - sent);
      await link.writer.write(data.subarray(sent, sent + n));
      sent += n;
    }
    const msg = await link.expect(10000, "ack", "fail");
    if (!msg || msg.startsWith("fail")) throw plain("The restore stopped halfway; the remote's setup is as it was. Try again.");
    acked = Number(msg.split(" ")[1]);
    $("set-status").textContent = `Restoring… ${Math.round((acked / data.length) * 100)}%`;
  }
  const end = await link.expect(20000, "done", "fail");
  if (end !== "done")
    throw plain(`The remote didn't take it (${end ? end.slice(5) : "no answer"}). Its setup is as it was.`);
  setResult("ok", "<b>Restored.</b> The remote is restarting with the settings from the backup.");
}

$("set-backup").addEventListener("click", async () => {
  if (busy) return;
  const pw = await askPassword({
    title: "Protect the backup",
    text: "The file is encrypted with this password, and restoring it asks for it. There's no way to get it back if it's lost. " +
          "Or save it without the Wi-Fi password and Home Assistant sign-in.",
    repeat: true, skip: true,
  });
  if (pw == null) return;
  settingsJob("Backing up…", async (link) => {
    const b = await backup(link);
    let name;
    if (pw) {
      $("set-status").textContent = "Encrypting…";
      name = saveFile(await encryptBackup(b, pw), "");
    } else {
      delete b.secrets;
      name = saveFile(b, "-no-secrets");
    }
    setResult("ok", `<b>Backed up.</b> Saved as <code>${name}</code> in your downloads. ` +
      (pw ? "It's encrypted: restoring it asks for the password."
          : "The Wi-Fi password and Home Assistant sign-in aren't in it: restoring it keeps the ones on the remote."));
  });
});

$("set-restore").addEventListener("click", () => $("set-file").click());

$("set-file").addEventListener("change", async () => {
  const f = $("set-file").files[0];
  $("set-file").value = "";
  if (!f) return;
  const data = new Uint8Array(await f.arrayBuffer());
  let info;
  try {
    info = JSON.parse(new TextDecoder().decode(data));
  } catch {}
  if (info && info.bleep_backup_encrypted === 1) {
    let opened = null;
    const pw = await askPassword({
      title: "Encrypted backup",
      text: `Type the password ${f.name} was saved with.`,
      check: async (p) => ((opened = await decryptBackup(info, p)) ? null : "That's not the password for this file."),
    });
    if (pw == null) return;
    info = opened;
  } else if (info && typeof info.bleep_backup_encrypted === "number") {
    setResult("err", "<b>This backup is from a newer page.</b> Reload this page and try again.");
    return;
  }
  if (!info || typeof info.bleep_backup !== "number" || typeof info.config !== "object") {
    setResult("err", `<b>That isn't a Bleep backup.</b> Pick the <code>bleep-settings-….json</code> file a backup saved.`);
    return;
  }
  if (!confirm(`Restore ${f.name}? It replaces everything set up on the remote now (devices, activities, ` +
               "routines, Home, Wi-Fi, Home Assistant), and the remote restarts." +
               (info.secrets ? "" : " This backup has no passwords in it: the remote keeps its Wi-Fi password and Home Assistant sign-in.")))
    return;
  const plainData = new TextEncoder().encode(JSON.stringify(info));
  settingsJob("Restoring…", (link) => restore(link, plainData));
});

/* ---------- console ---------- */

const con = { port: null, reader: null, want: false, loop: null };
const MAX_CONSOLE = 300000;   // characters kept on screen

function conPrint(text) {
  const el = $("console-out");
  const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 40;
  el.textContent += text;
  if (el.textContent.length > MAX_CONSOLE) el.textContent = el.textContent.slice(-MAX_CONSOLE * 0.8);
  if (atBottom) el.scrollTop = el.scrollHeight;
}

function conButtons() {
  const open = !!con.reader;
  $("con-connect").textContent = con.want ? "disconnect" : "connect";
  $("con-restart").disabled = !open;
  $("con-save").disabled = $("con-copy").disabled = !$("console-out").textContent;
}

async function conOpen(port) {
  await port.open({ baudRate: 115200 });
  con.port = port;
  con.reader = port.readable.getReader();
  conButtons();
  const decoder = new TextDecoder();
  con.loop = (async () => {
    try {
      for (;;) {
        const { value, done } = await con.reader.read();
        if (done) break;
        conPrint(decoder.decode(value, { stream: true }));
        $("con-save").disabled = $("con-copy").disabled = false;
      }
    } catch {
      /* the remote restarted or was unplugged */
    } finally {
      try { con.reader.releaseLock(); } catch {}
      try { await port.close(); } catch {}
      con.reader = null;
      conButtons();
      if (con.want) conPrint("\n— waiting for the remote to come back —\n");
    }
  })();
}

async function consoleStart() {
  let port;
  try {
    port = await navigator.serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF_VID }] });
  } catch {
    return;
  }
  con.want = true;
  try {
    await conOpen(port);
    conPrint("— connected —\n");
  } catch (err) {
    con.want = false;
    conPrint("Couldn't open it: " + friendly(err) + "\n");
  }
  conButtons();
}

async function consoleStop() {
  con.want = false;
  if (con.reader) {
    try { await con.reader.cancel(); } catch {}
    await con.loop;
  }
  conButtons();
}

// A restart re-enumerates the remote's USB: open it again when it's back
navigator.serial?.addEventListener("connect", async (e) => {
  if (!con.want || con.reader || busy) return;
  const info = e.target.getInfo?.() || {};
  if (info.usbVendorId !== ESPRESSIF_VID) return;
  try {
    await conOpen(e.target);
    conPrint("— reconnected —\n");
  } catch { /* still starting: the next connect event tries again */ }
});

$("con-connect").addEventListener("click", () => (con.want ? consoleStop() : consoleStart()));

$("con-restart").addEventListener("click", async () => {
  if (!con.port) return;
  // RTS resets the chip; DTR stays low so it starts normally, not in download mode
  try {
    await con.port.setSignals({ dataTerminalReady: false, requestToSend: true });
    await new Promise((r) => setTimeout(r, 100));
    await con.port.setSignals({ requestToSend: false });
    conPrint("\n— restarting —\n");
  } catch (err) {
    conPrint("\nCouldn't restart it: " + friendly(err) + "\n");
  }
});

$("con-clear").addEventListener("click", () => {
  $("console-out").textContent = "";
  conButtons();
});

/* the whole log to the clipboard, to paste where it's asked for */
$("con-copy").addEventListener("click", async () => {
  const btn = $("con-copy");
  const text = $("console-out").textContent;
  let ok = false;
  try {
    await navigator.clipboard.writeText(text);
    ok = true;
  } catch {
    /* no clipboard API (or not allowed): select it and copy the old way */
    const ta = document.createElement("textarea");
    ta.value = text;
    ta.style.position = "fixed";
    ta.style.opacity = "0";
    document.body.appendChild(ta);
    ta.select();
    try { ok = document.execCommand("copy"); } catch {}
    ta.remove();
  }
  btn.textContent = ok ? "copied" : "couldn't copy";
  clearTimeout(btn._t);
  btn._t = setTimeout(() => (btn.textContent = "copy"), 1500);
});

$("con-save").addEventListener("click", () => {
  const blob = new Blob([$("console-out").textContent], { type: "text/plain" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `bleep-log-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.txt`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
});

loadManifest();
