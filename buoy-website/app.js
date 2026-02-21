import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-app.js";
import { getDatabase, ref, onValue, query, limitToLast } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-database.js";

const firebaseConfig = {
  apiKey: "AIzaSyByUS64PQ41yKp1K003DbhaBJ4E_IM1lyo",
  authDomain: "marine-buoy.firebaseapp.com",
  databaseURL: "https://marine-buoy-default-rtdb.firebaseio.com",
  projectId: "marine-buoy",
  storageBucket: "marine-buoy.firebasestorage.app",
  messagingSenderId: "126380799821",
  appId: "1:126380799821:web:2ad49f7dfe0d3fb2f7a904"
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

// ---------- Helpers ----------
function getForecastEmoji(forecast = "") {
  const f = forecast.toLowerCase();

  if (f.includes("thunder")) return "⛈️";
  if (f.includes("rain") || f.includes("showers")) return "🌧️";
  if (f.includes("snow")) return "❄️";
  if (f.includes("cloud")) return "☁️";
  if (f.includes("fog") || f.includes("haze")) return "🌫️";
  if (f.includes("wind")) return "💨";
  if (f.includes("sun") || f.includes("clear")) return "☀️";
  if (f.includes("partly")) return "⛅";
  return "🌤️";
}

function getStatusDescription(status = "") {
  const s = status.toLowerCase();

  if (s === "good") {
    return "Wave conditions look favorable for surfers. Expect more consistent motion with better surf potential.";
  }
  if (s === "ok") {
    return "Conditions are moderate. Surf may be rideable, but quality can vary depending on wind and local conditions.";
  }
  if (s === "bad") {
    return "Conditions are rough or not ideal for surfing right now. Strong wind, unstable waves, or poor forecast may affect safety and ride quality.";
  }
  return "Status description unavailable.";
}

function applyStatusStyle(statusEl, status = "") {
  statusEl.classList.remove("status-good", "status-ok", "status-bad");
  const s = status.toLowerCase();
  if (s === "good") statusEl.classList.add("status-good");
  else if (s === "ok") statusEl.classList.add("status-ok");
  else if (s === "bad") statusEl.classList.add("status-bad");
}

// Parse date + 12-hour time for better sorting
function toDateObj(dateStr, timeStr) {
  if (!dateStr || !timeStr) return new Date(0);
  // Example: 2026-02-19 + 7:51:13 PM
  const combined = `${dateStr} ${timeStr}`;
  const d = new Date(combined);
  return Number.isNaN(d.getTime()) ? new Date(0) : d;
}

// ---------- Latest ----------
const latestRef = ref(db, "buoy/latest");

onValue(
  latestRef,
  (snapshot) => {
    const data = snapshot.val();
    const lastUpdatedEl = document.getElementById("lastUpdated");

    if (!data) {
      lastUpdatedEl.textContent = "No data found at /buoy/latest";
      return;
    }

    // Map latest values
    document.getElementById("temperatureF").textContent = data.temperatureF ?? "—";
    document.getElementById("humidity").textContent = data.humidity ?? "—";
    document.getElementById("weatherForecast").textContent = data.weatherForecast ?? "—";
    document.getElementById("windMph").textContent = data.windMph ?? "—";
    document.getElementById("gustMph").textContent = data.gustMph ?? "—";
    document.getElementById("windDirection").textContent = data.windDirection ?? "—";
    document.getElementById("rms").textContent = data.rms ?? "—";

    // Time and date
    const date = data.date ?? "";
    const time = data.time ?? "";
    document.getElementById("timeDateText").textContent =
      date && time ? `${time} • ${date}` : (date || time || "—");

    // Buoy status
    const status = data.buoyStatus ?? "—";
    const statusEl = document.getElementById("buoyStatus");
    statusEl.textContent = status;
    applyStatusStyle(statusEl, status);

    // Description
    document.getElementById("statusDescription").textContent = getStatusDescription(status);

    // Forecast icon
    document.getElementById("forecastIcon").textContent = getForecastEmoji(data.weatherForecast ?? "");

    // Footer
    lastUpdatedEl.textContent = "Live update: " + new Date().toLocaleString();
  },
  (err) => {
    console.error("Latest read error:", err);
    document.getElementById("lastUpdated").textContent =
      "Firebase error loading latest data (check rules/config).";
  }
);

// ---------- History ----------
const historyListEl = document.getElementById("historyList");
const logsQ = query(ref(db, "buoy/logs"), limitToLast(12));

onValue(
  logsQ,
  (snapshot) => {
    const logsObj = snapshot.val();

    if (!logsObj) {
      historyListEl.innerHTML = `<div class="history-empty">No history logs found.</div>`;
      return;
    }

    const rows = Object.entries(logsObj).map(([id, v]) => ({ id, ...v }));

    // Sort newest first using parsed Date object (works with AM/PM)
    rows.sort((a, b) => {
      const aDate = toDateObj(a.date, a.time);
      const bDate = toDateObj(b.date, b.time);
      return bDate - aDate;
    });

    historyListEl.innerHTML = rows.map((r) => {
      const ts = (r.date && r.time) ? `${r.time} • ${r.date}` : (r.date || r.time || "—");
      return `
        <div class="history-row">
          <div class="history-left">
            <span class="pill">${r.buoyStatus ?? "—"}</span>
            <span><strong>Forecast:</strong> ${r.weatherForecast ?? "—"}</span>
            <span><strong>T:</strong> ${r.temperatureF ?? "—"}°F</span>
            <span><strong>H:</strong> ${r.humidity ?? "—"}%</span>
            <span><strong>W:</strong> ${r.windMph ?? "—"} mph</span>
            <span><strong>G:</strong> ${r.gustMph ?? "—"} mph</span>
            <span><strong>Dir:</strong> ${r.windDirection ?? "—"}</span>
            <span><strong>RMS:</strong> ${r.rms ?? "—"}</span>
          </div>
          <div class="history-time">${ts}</div>
        </div>
      `;
    }).join("");
  },
  (err) => {
    console.error("History read error:", err);
    historyListEl.innerHTML =
      `<div class="history-empty">Can't load history logs (check rules).</div>`;
  }
);