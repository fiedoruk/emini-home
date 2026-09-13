/* emini Home pure helpers and optional local startup guard. No network calls. */
(function (root) {
  "use strict";
  function createBootGuard(host) {
    if (!host.document || !host.addEventListener || !host.setTimeout)
      return null;
    var done = false,
      timer;
    function fail() {
      if (done) return;
      var panel = host.document.getElementById("boot-panel");
      if (!panel) return;
      var title = host.document.getElementById("boot-title"),
        message = host.document.getElementById("boot-message");
      if (title)
        title.textContent =
          "The panel is not ready yet / Panel jeszcze nie jest gotowy";
      if (message)
        message.textContent =
          "Check the Wi-Fi and address below, then reload. / Sprawdź sieć Wi-Fi i adres poniżej, a potem wczytaj panel ponownie.";
      panel.setAttribute("data-boot-state", "recovery");
    }
    function ready() {
      done = true;
      host.clearTimeout(timer);
      host.removeEventListener("error", fail, true);
      host.removeEventListener("unhandledrejection", fail);
    }
    host.addEventListener("error", fail, true);
    host.addEventListener("unhandledrejection", fail);
    timer = host.setTimeout(fail, 8000);
    return { ready: ready, fail: fail };
  }
  const boot = createBootGuard(root);
  if (boot) root.HomeBoot = boot;
  const screens = ["weather", "feed", "note"];
  // "cycle" = Print, Rhythm and Atlas take turns (D-HOME-CC-18). Previews never send it.
  const styles = ["print", "rhythm", "atlas", "cycle"];
  const geocoder = "https://geocoding-api.open-meteo.com/v1/search";
  const zones = [
    "UTC",
    "Europe/Warsaw",
    "Europe/London",
    "Europe/Berlin",
    "America/New_York",
    "America/Chicago",
    "America/Los_Angeles",
    "Asia/Tokyo",
    "Asia/Shanghai",
    "Asia/Kolkata",
    "Australia/Sydney",
  ];
  const palette = [
    [26, 26, 22],
    [230, 229, 219],
    [247, 173, 1],
    [123, 0, 1],
  ];
  const recipeKeys = [
    "styles",
    "texture",
    "intensity",
    "large_text",
    "order",
    "enabled",
    "mode",
    "fixed_screen",
    "interval_min",
    "pause_min",
    "cycle_min",
    "day",
    "quiet",
    "weekdays",
    "locale",
    "units",
    "clock24",
  ];
  const clone = (x) => JSON.parse(JSON.stringify(x));
  const bytes = (x) => new TextEncoder().encode(x).length;
  const noteUsage = (value) => ({
    percent: Math.ceil((bytes(value) / 240) * 100),
    overfull: bytes(value) > 240,
  });
  function formatTimestamp(epoch, locale, zone, clock24) {
    if (!Number.isFinite(epoch) || epoch <= 0) return null;
    if (typeof zone !== "string" || !zone) zone = "UTC";
    const date = new Date(epoch * 1000);
    if (!Number.isFinite(date.getTime())) return null;
    const options = {
      dateStyle: "short",
      timeStyle: "short",
      timeZone: zone,
      hourCycle: clock24 ? "h23" : "h12",
    };
    try {
      return {
        text: new Intl.DateTimeFormat(locale, options).format(date),
        zone,
        fallback: false,
      };
    } catch {
      try {
        return {
          text:
            new Intl.DateTimeFormat(locale, {
              ...options,
              timeZone: "UTC",
            }).format(date) + " UTC",
          zone: "UTC",
          fallback: true,
        };
      } catch {
        return {
          text: date.toISOString().slice(0, 16).replace("T", " ") + " UTC",
          zone: "UTC",
          fallback: true,
        };
      }
    }
  }
  function localDeviceURL(address) {
    if (typeof address !== "string" || !/^\d{1,3}(\.\d{1,3}){3}$/.test(address))
      return null;
    const p = address.split(".").map(Number);
    if (p.some((n) => n > 255)) return null;
    if (!(
      p[0] === 10 ||
      (p[0] === 172 && p[1] >= 16 && p[1] <= 31) ||
      (p[0] === 192 && p[1] === 168)
    ))
      return null;
    return "http://" + p.join(".") + "/";
  }
  function lanHandoffState(hostname, address, phoneChangedNetwork) {
    const url = localDeviceURL(address);
    if (!url) return { url: null, same: false, allowed: false };
    const same = new URL(url).hostname === hostname;
    return { url, same, allowed: same || phoneChangedNetwork === true };
  }
  function batteryView(battery) {
    const b = battery || {};
    const volts =
      b.valid === true && Number.isFinite(b.millivolts) && b.millivolts > 0
        ? b.millivolts / 1000
        : null;
    const percent =
      volts !== null &&
      b.charging === false &&
      Number.isFinite(b.percent_estimate) &&
      b.percent_estimate >= 0 &&
      b.percent_estimate <= 100
        ? Math.round(b.percent_estimate)
        : null;
    const kind =
      b.full === true
        ? "full"
        : b.charging === true
          ? "charging"
          : b.charging === false && b.full === false
            ? "not_charging"
            : "unknown";
    return { volts, percent, kind };
  }
  function placeCandidates(raw) {
    if (!Array.isArray(raw)) throw new Error("places_schema");
    return raw
      .slice(0, 50)
      .filter(
        (p) =>
          p &&
          typeof p.name === "string" &&
          p.name.trim() &&
          bytes(p.name) <= 64 &&
          typeof p.latitude === "number" &&
          Number.isFinite(p.latitude) &&
          Math.abs(p.latitude) <= 90 &&
          typeof p.longitude === "number" &&
          Number.isFinite(p.longitude) &&
          Math.abs(p.longitude) <= 180 &&
          typeof p.country === "string" &&
          /^[A-Z]{2}$/.test(p.country),
      )
      .map((p) => ({
        name: p.name,
        country: p.country,
        latitude: p.latitude,
        longitude: p.longitude,
        timezone:
          typeof p.timezone === "string" && p.timezone.length <= 48
            ? p.timezone
            : "",
      }));
  }
  function areaSuggestion(raw) {
    if (
      !raw ||
      raw.state !== "ready" ||
      raw.provider !== "FreeIPAPI" ||
      raw.accuracy !== "approximate"
    )
      throw new Error("location_schema");
    const matches = placeCandidates([
      {
        name: raw.city,
        country: raw.country,
        latitude: raw.latitude,
        longitude: raw.longitude,
        timezone: raw.timezone,
      },
    ]);
    if (matches.length !== 1) throw new Error("location_schema");
    return { ...matches[0], provider: "FreeIPAPI", accuracy: "approximate" };
  }
  function applyPlace(config, place) {
    const c = clone(config);
    c.location = place.name;
    c.latitude = place.latitude;
    c.longitude = place.longitude;
    if (zones.includes(place.timezone)) c.timezone = place.timezone;
    return c;
  }
  function locationFingerprint(config) {
    const text = JSON.stringify(config);
    let a = 2166136261,
      b = 2246822507;
    for (let i = 0; i < text.length; i++) {
      a = Math.imul(a ^ text.charCodeAt(i), 16777619);
      b = Math.imul(b ^ text.charCodeAt(i), 3266489909);
    }
    return (
      (a >>> 0).toString(16).padStart(8, "0") +
      (b >>> 0).toString(16).padStart(8, "0")
    );
  }
  function locationUnchanged(config, pending) {
    return (
      !!config &&
      config.revision === pending.revision &&
      locationFingerprint(config) === pending.fingerprint
    );
  }
  function automaticLocation(config, pending, position) {
    if (!locationUnchanged(config, pending))
      throw new Error("location_conflict");
    const place = areaSuggestion(position);
    place.timezone = pending.timezone || "";
    const next = applyPlace(config, place);
    next.location_ready = true;
    return next;
  }
  function zoneMismatch(areaZone, phoneZone) {
    return (
      typeof areaZone === "string" &&
      typeof phoneZone === "string" &&
      !!areaZone &&
      !!phoneZone &&
      areaZone !== phoneZone
    );
  }
  /* Town search (layer 2): the phone asks Open-Meteo Geocoding directly. */
  function geocodeURL(query, lang) {
    if (typeof query !== "string") return null;
    const name = query.trim().slice(0, 100);
    if ([...name].length < 2) return null;
    const params = new URLSearchParams({
      name,
      count: "6",
      language: lang === "pl" ? "pl" : "en",
      format: "json",
    });
    return geocoder + "?" + params.toString();
  }
  function truncateUTF8(value, limit) {
    // Control characters are refused by the firmware codec; lone surrogates are not UTF-8.
    // Code points come from the string iterator, so a surrogate pair is never split.
    let out = "",
      used = 0;
    for (const ch of String(value ?? "").trim()) {
      const unit = ch.charCodeAt(0);
      if (
        unit < 32 ||
        unit === 127 ||
        (ch.length === 1 && unit >= 0xd800 && unit <= 0xdfff)
      )
        continue;
      const size = bytes(ch);
      if (used + size > limit) break;
      out += ch;
      used += size;
    }
    return out.trim();
  }
  function roundCoordinate(value) {
    const rounded = Math.round(value * 10000) / 10000;
    return rounded === 0 ? 0 : rounded;
  }
  function geocodeResults(payload, known = zones) {
    if (
      !payload ||
      typeof payload !== "object" ||
      Array.isArray(payload) ||
      payload.error
    )
      throw new Error("geocode_schema");
    if (payload.results === undefined) return [];
    if (!Array.isArray(payload.results)) throw new Error("geocode_schema");
    const label = (x) => (typeof x === "string" ? truncateUTF8(x, 96) : "");
    const rows = payload.results
      .slice(0, 6)
      .filter(
        (p) =>
          p &&
          typeof p.latitude === "number" &&
          Number.isFinite(p.latitude) &&
          Math.abs(p.latitude) <= 90 &&
          typeof p.longitude === "number" &&
          Number.isFinite(p.longitude) &&
          Math.abs(p.longitude) <= 180 &&
          typeof p.name === "string" &&
          truncateUTF8(p.name, 64),
      )
      .map((p) => {
        const zone = typeof p.timezone === "string" ? p.timezone : "";
        return {
          name: truncateUTF8(p.name, 64),
          // "województwo zachodniopomorskie" reads as "zachodniopomorskie" next to the country.
          region: label(p.admin1).replace(/^województwo\s+/i, ""),
          district: label(p.admin2),
          country: label(p.country) || label(p.country_code),
          latitude: roundCoordinate(p.latitude),
          longitude: roundCoordinate(p.longitude),
          timezone: known.includes(zone) ? zone : "",
        };
      });
    // Two places with the same name, region and country also show their district.
    const detail = (p, more) =>
      [more && p.district, p.region, p.country].filter(Boolean).join(" · ");
    const plain = rows.map((p) => p.name + " · " + detail(p, false));
    return rows.map((p, i) => {
      const more = plain.indexOf(plain[i]) !== plain.lastIndexOf(plain[i]),
        rest = detail(p, more);
      return {
        ...p,
        detail: rest,
        label: rest ? p.name + " · " + rest : p.name,
      };
    });
  }
  function searchPlace(config, place) {
    const next = applyPlace(config, {
      name: truncateUTF8(place.name, 64),
      latitude: roundCoordinate(place.latitude),
      longitude: roundCoordinate(place.longitude),
      timezone: place.timezone,
    });
    next.location_ready = true;
    return next;
  }
  const placeKey = (c) =>
    c
      ? JSON.stringify([c.location, c.latitude, c.longitude, c.location_ready])
      : "";
  function previewConfig(config) {
    // The device renders one concrete composition; "cycle" previews as Print, the first in turn.
    const c = clone(config);
    if (c && c.styles && typeof c.styles === "object")
      for (const s of screens)
        if (c.styles[s] === "cycle") c.styles[s] = "print";
    return c;
  }
  function sourcesKey(status) {
    const s = status && status.sources;
    if (!s || typeof s !== "object") return null;
    return JSON.stringify(
      ["weather", "feed"].map((k) => [
        s[k]?.fetched_at ?? null,
        s[k]?.valid ?? null,
      ]),
    );
  }
  function quantizeRGBA(input) {
    if (
      !(input instanceof Uint8Array || input instanceof Uint8ClampedArray) ||
      input.length % 4
    )
      throw new Error("rgba_size");
    const result = new Uint8ClampedArray(input.length);
    for (let i = 0; i < input.length; i += 4) {
      const alpha = input[i + 3] / 255;
      const colour = [0, 1, 2].map(
        (c) => input[i + c] * alpha + palette[1][c] * (1 - alpha),
      );
      let best = 0,
        distance = Infinity;
      palette.forEach((p, index) => {
        const d = p.reduce((sum, c, k) => sum + (c - colour[k]) ** 2, 0);
        if (d < distance) {
          distance = d;
          best = index;
        }
      });
      result.set([...palette[best], 255], i);
    }
    return result;
  }
  function normalizeNetworks(payload) {
    if (
      !payload ||
      !["idle", "scanning", "ready", "error"].includes(payload.state) ||
      !Array.isArray(payload.networks)
    )
      throw new Error("scan_schema");
    const byName = new Map();
    for (const row of payload.networks.slice(0, 100)) {
      if (
        !row ||
        typeof row.ssid !== "string" ||
        !row.ssid.length ||
        bytes(row.ssid) > 32 ||
        /[\u0000-\u001f\u007f\ud800-\udfff]/u.test(row.ssid) ||
        !Number.isFinite(row.rssi) ||
        row.rssi > 0 ||
        row.rssi < -150 ||
        typeof row.secure !== "boolean" ||
        typeof row.supported !== "boolean" ||
        typeof row.connected !== "boolean"
      )
        continue;
      const item = {
        ssid: row.ssid,
        rssi: row.rssi,
        secure: row.secure,
        supported: row.supported && row.secure,
        connected: row.connected,
      };
      const old = byName.get(item.ssid);
      if (
        !old ||
        (Number(item.connected) - Number(old.connected) ||
          Number(item.supported) - Number(old.supported) ||
          item.rssi - old.rssi) > 0
      )
        byName.set(item.ssid, item);
    }
    return {
      state: payload.state,
      networks: [...byName.values()]
        .sort(
          (a, b) =>
            Number(b.connected) - Number(a.connected) ||
            Number(b.supported) - Number(a.supported) ||
            b.rssi - a.rssi ||
            a.ssid.localeCompare(b.ssid),
        )
        .slice(0, 20),
      error: typeof payload.error === "string" ? payload.error : "",
    };
  }
  const signalLevel = (rssi) => (rssi >= -55 ? 3 : rssi >= -67 ? 2 : 1);
  function wifiOutcome(status, expectedSSID, awaitingFreshStatus, timedOut) {
    if (awaitingFreshStatus) return timedOut ? "unconfirmed" : "connecting";
    const wifi = status?.wifi;
    if (
      wifi?.state === "connected" &&
      (!expectedSSID || wifi.ssid === expectedSSID)
    )
      return "connected";
    if (
      wifi?.state === "error" &&
      (!expectedSSID || !wifi.ssid || wifi.ssid === expectedSSID)
    )
      return "error";
    return timedOut ? "unconfirmed" : "connecting";
  }
  function decodeFrame(input) {
    const b = input instanceof Uint8Array ? input : new Uint8Array(input);
    if (b.length !== 30000) throw new Error("frame_size");
    const rgba = new Uint8ClampedArray(400 * 300 * 4);
    for (let i = 0; i < 120000; i++) {
      const c = palette[(b[i >> 2] >> (6 - (i % 4) * 2)) & 3];
      rgba.set([c[0], c[1], c[2], 255], i * 4);
    }
    return rgba;
  }
  function validate(c, recipe = false) {
    const errors = [];
    const has = (k) => Object.prototype.hasOwnProperty.call(c, k);
    const check = (k, ok) => {
      if ((!recipe || has(k)) && !ok) errors.push(k);
    };
    if (!c || typeof c !== "object" || Array.isArray(c)) return ["schema"];
    check("schema", c.schema === 1);
    if (!recipe) {
      check("revision", Number.isInteger(c.revision) && c.revision >= 0);
      for (const [k, n] of [
        ["name", 48],
        ["location", 64],
        ["note", 240],
        ["feed_url", 512],
      ])
        check(k, typeof c[k] === "string" && bytes(c[k]) <= n);
      check(
        "latitude",
        typeof c.latitude === "number" &&
          Number.isFinite(c.latitude) &&
          Math.abs(c.latitude) <= 90,
      );
      check(
        "longitude",
        typeof c.longitude === "number" &&
          Number.isFinite(c.longitude) &&
          Math.abs(c.longitude) <= 180,
      );
      check("timezone", zones.includes(c.timezone));
      check(
        "location_ready",
        c.location_ready === undefined || typeof c.location_ready === "boolean",
      );
      if (c.feed_url) {
        try {
          const u = new URL(c.feed_url);
          if (u.protocol !== "https:" || u.username || u.password)
            errors.push("feed_url");
        } catch {
          errors.push("feed_url");
        }
      }
    }
    check("fixed_screen", screens.includes(c.fixed_screen));
    check(
      "pause_min",
      Number.isInteger(c.pause_min) && c.pause_min >= 0 && c.pause_min <= 240,
    );
    check(
      "cycle_min",
      (recipe && !has("cycle_min")) ||
        (Number.isInteger(c.cycle_min) &&
          c.cycle_min >= 5 &&
          c.cycle_min <= 1440),
    );
    check(
      "weekdays",
      Number.isInteger(c.weekdays) && c.weekdays >= 1 && c.weekdays <= 127,
    );
    check("locale", ["en", "pl"].includes(c.locale));
    check("units", ["C", "F"].includes(c.units));
    check("texture", [1, 2, 4].includes(c.texture));
    check("intensity", [0, 1, 2].includes(c.intensity));
    check("large_text", typeof c.large_text === "boolean");
    check("clock24", typeof c.clock24 === "boolean");
    check("mode", ["fixed", "day", "rotate"].includes(c.mode));
    check(
      "interval_min",
      Number.isInteger(c.interval_min) &&
        c.interval_min >= 5 &&
        c.interval_min <= 1440,
    );
    check(
      "enabled",
      Array.isArray(c.enabled) &&
        c.enabled.length === 3 &&
        c.enabled.every((x) => typeof x === "boolean") &&
        c.enabled.some(Boolean),
    );
    check(
      "order",
      Array.isArray(c.order) &&
        c.order.length === 3 &&
        new Set(c.order).size === 3 &&
        c.order.every((s) => screens.includes(s)),
    );
    check(
      "styles",
      c.styles &&
        typeof c.styles === "object" &&
        screens.every((s) => styles.includes(c.styles[s])) &&
        Object.keys(c.styles).length === 3,
    );
    const time = (x) =>
      typeof x === "string" && /^([01]\d|2[0-3]):[0-5]\d$/.test(x);
    check(
      "quiet",
      c.quiet &&
        typeof c.quiet.enabled === "boolean" &&
        time(c.quiet.start) &&
        time(c.quiet.end),
    );
    check(
      "day",
      Array.isArray(c.day) &&
        c.day.length === 3 &&
        c.day.every(
          (d, i) =>
            d &&
            time(d.time) &&
            screens.includes(d.screen) &&
            (i === 0 || c.day[i - 1].time < d.time),
        ),
    );
    return [...new Set(errors)];
  }
  function safeRecipe(raw) {
    if (
      !raw ||
      typeof raw !== "object" ||
      Array.isArray(raw) ||
      raw.schema !== 1
    )
      throw new Error("recipe_schema");
    const out = { schema: 1 };
    for (const k of recipeKeys) {
      if (!Object.prototype.hasOwnProperty.call(raw, k)) {
        if (k === "cycle_min") continue; // recipes from before 0.4.0; Home keeps its value
        throw new Error("recipe_missing");
      }
      out[k] = clone(raw[k]);
    }
    // Reject nested baggage as well as secrets in otherwise recognised fields.
    if (
      Object.keys(out.quiet || {}).some(
        (k) => !["enabled", "start", "end"].includes(k),
      ) ||
      !Array.isArray(out.day) ||
      out.day.some(
        (d) =>
          !d || Object.keys(d).some((k) => !["time", "screen"].includes(k)),
      )
    )
      throw new Error("recipe_fields");
    const errors = validate(out, true);
    if (errors.length) throw new Error("recipe_invalid:" + errors.join(","));
    return out;
  }
  function profile(config, name) {
    const c = clone(config);
    if (name === "desk") {
      c.large_text = false;
      c.texture = 1;
      c.intensity = 1;
      c.mode = "day";
    } else if (name === "distance") {
      c.large_text = true;
      c.texture = 2;
      c.intensity = 1;
      c.mode = "fixed";
    } else if (name === "showcase") {
      c.large_text = false;
      c.texture = 1;
      c.intensity = 2;
      c.mode = "rotate";
      c.interval_min = 30;
      c.enabled = [true, true, false];
      c.order = ["weather", "feed", "note"];
    } else throw new Error("profile");
    return c;
  }
  const api = {
    screens,
    styles,
    geocoder,
    zones,
    palette,
    recipeKeys,
    clone,
    bytes,
    noteUsage,
    formatTimestamp,
    localDeviceURL,
    lanHandoffState,
    batteryView,
    createBootGuard,
    placeCandidates,
    areaSuggestion,
    applyPlace,
    locationFingerprint,
    locationUnchanged,
    automaticLocation,
    zoneMismatch,
    geocodeURL,
    truncateUTF8,
    roundCoordinate,
    geocodeResults,
    searchPlace,
    placeKey,
    previewConfig,
    sourcesKey,
    quantizeRGBA,
    normalizeNetworks,
    signalLevel,
    wifiOutcome,
    decodeFrame,
    validate,
    safeRecipe,
    profile,
  };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.HomeCore = api;
})(typeof globalThis !== "undefined" ? globalThis : this);
