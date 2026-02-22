// ===================== FIREBASE IMPORTS =====================
// firebase-app initializes your app
// firebase-database lets you read from Realtime Database
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-app.js";
import { getDatabase, ref, onValue, query, limitToLast } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-database.js";

// ===================== FIREBASE CONFIG =====================
// This connects your website to YOUR Firebase project
const firebaseConfig = {
  apiKey: "AIzaSyByUS64PQ41yKp1K003DbhaBJ4E_IM1lyo",
  authDomain: "marine-buoy.firebaseapp.com",
  databaseURL: "https://marine-buoy-default-rtdb.firebaseio.com",
  projectId: "marine-buoy",
  storageBucket: "marine-buoy.firebasestorage.app",
  messagingSenderId: "126380799821",
  appId: "1:126380799821:web:2ad49f7dfe0d3fb2f7a904"
};

// Create Firebase app instance + database instance
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

// ===================== CHART SETUP =====================
// Store chart instance so we can update it live instead of recreating it
let windChart = null;

/**
 * Custom Chart.js plugin:
 * Draws arrowheads on each line segment so the line looks directional
 * (similar to the surf/wind graph style you showed).
 */
const arrowLinePlugin = {
  id: "arrowLinePlugin",

  // Runs after Chart.js draws the line dataset
  afterDatasetsDraw(chart) {
    const { ctx } = chart;

    // Loop through all datasets (we currently use one line dataset)
    chart.data.datasets.forEach((dataset, datasetIndex) => {
      const meta = chart.getDatasetMeta(datasetIndex);

      // Skip if not a visible line dataset
      if (!meta || meta.hidden || meta.type !== "line") return;

      const points = meta.data;
      if (!points || points.length < 2) return; // Need at least 2 points for arrows

      ctx.save();
      ctx.strokeStyle = dataset.borderColor || "#2563eb";
      ctx.fillStyle = dataset.borderColor || "#2563eb";
      ctx.lineWidth = 2;

      // Draw an arrowhead for each segment between points
      for (let i = 0; i < points.length - 1; i++) {
        const p1 = points[i];
        const p2 = points[i + 1];
        if (!p1 || !p2) continue;

        const x1 = p1.x;
        const y1 = p1.y;
        const x2 = p2.x;
        const y2 = p2.y;

        // Angle of the line segment
        const angle = Math.atan2(y2 - y1, x2 - x1);

        // Arrowhead size
        const arrowLength = 10;
        const arrowWidth = 5;

        // Pull arrowhead back so it doesn't overlap the point marker
        const backOff = 8;
        const ax = x2 - Math.cos(angle) * backOff;
        const ay = y2 - Math.sin(angle) * backOff;

        // Draw triangle arrowhead
        ctx.beginPath();
        ctx.moveTo(ax, ay);
        ctx.lineTo(
          ax - arrowLength * Math.cos(angle) + arrowWidth * Math.sin(angle),
          ay - arrowLength * Math.sin(angle) - arrowWidth * Math.cos(angle)
        );
        ctx.lineTo(
          ax - arrowLength * Math.cos(angle) - arrowWidth * Math.sin(angle),
          ay - arrowLength * Math.sin(angle) + arrowWidth * Math.cos(angle)
        );
        ctx.closePath();
        ctx.fill();
      }

      ctx.restore();
    });
  }
};

// Register plugin once (Chart.js is loaded in index.html via CDN script tag)
if (window.Chart) {
  Chart.register(arrowLinePlugin);
}

// ===================== HELPER FUNCTIONS =====================
// These are small reusable functions to keep the code cleaner

/**
 * Convert forecast text into an emoji.
 * Example:
 * - "Chance Rain Showers" -> 🌧️
 * - "Sunny" -> ☀️
 */
function getForecastEmoji(forecast = "") {
  const f = String(forecast).toLowerCase();

  if (f.includes("thunder")) return "⛈️";
  if (f.includes("rain") || f.includes("showers")) return "🌧️";
  if (f.includes("snow")) return "❄️";
  if (f.includes("cloud")) return "☁️";
  if (f.includes("fog") || f.includes("haze")) return "🌫️";
  if (f.includes("wind")) return "💨";
  if (f.includes("sun") || f.includes("clear")) return "☀️";
  if (f.includes("partly")) return "⛅";
  return "🌤️"; // default fallback
}

/**
 * Create a human-readable description based on buoyStatus.
 * This text appears in the description box.
 */
function getStatusDescription(status = "") {
  const s = String(status).toLowerCase();

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

/**
 * Apply CSS class to status badge so it changes color
 * (green = good, orange = ok, red = bad)
 */
function applyStatusStyle(statusEl, status = "") {
  if (!statusEl) return;

  // Remove any old status color class first
  statusEl.classList.remove("status-good", "status-ok", "status-bad");

  // Add the correct one
  const s = String(status).toLowerCase();
  if (s === "good") statusEl.classList.add("status-good");
  else if (s === "ok") statusEl.classList.add("status-ok");
  else if (s === "bad") statusEl.classList.add("status-bad");
}

/**
 * Convert date/time strings into a JS Date object for sorting.
 * Works with:
 * date = "2026-02-21"
 * time = "1:07:04 PM"
 */
function toDateObj(dateStr, timeStr) {
  if (!dateStr || !timeStr) return new Date(0); // fallback if missing

  // Build a combined string the Date parser can read
  const combined = `${dateStr} ${timeStr}`;
  const d = new Date(combined);

  // If parsing fails, return a very old date so it sorts last
  return Number.isNaN(d.getTime()) ? new Date(0) : d;
}

/**
 * Format main dashboard date as:
 * "Saturday, February 21, 2026"
 */
function formatLongDate(dateStr) {
  if (!dateStr) return "—";

  const d = new Date(`${dateStr}T00:00:00`);
  if (Number.isNaN(d.getTime())) return dateStr;

  return d.toLocaleDateString(undefined, {
    weekday: "long",
    year: "numeric",
    month: "long",
    day: "numeric"
  });
}

/**
 * Format history date as:
 * "02/21/26"
 */
function formatShortDate(dateStr) {
  if (!dateStr) return "—";

  const d = new Date(`${dateStr}T00:00:00`);
  if (Number.isNaN(d.getTime())) return dateStr;

  return d.toLocaleDateString(undefined, {
    year: "2-digit",
    month: "2-digit",
    day: "2-digit"
  });
}

/**
 * Create or update the wind-speed chart.
 *
 * @param {string[]} labels - X-axis labels (times from history logs)
 * @param {number[]} values - Wind speed values (mph)
 */
function renderWindChart(labels, values) {
  // Get canvas from index.html
  const canvas = document.getElementById("windChart");

  // Safety check: stop if canvas or Chart.js isn't available
  if (!canvas || !window.Chart) return;

  const ctx = canvas.getContext("2d");

  // If chart already exists, only update the data
  if (windChart) {
    windChart.data.labels = labels;
    windChart.data.datasets[0].data = values;
    windChart.update();
    return;
  }

  // Create chart for the first time
  windChart = new Chart(ctx, {
    type: "line",
    data: {
      labels, // x-axis labels
      datasets: [
        {
          label: "Wind Speed (mph)",
          data: values, // y-axis values
          borderColor: "#2563eb",
          backgroundColor: "rgba(37, 99, 235, 0.12)",
          fill: true,          // light shaded area under the line
          tension: 0.25,       // slight curve (0 = straight line)
          borderWidth: 2,

          // Point styling
          pointRadius: 4,
          pointHoverRadius: 5,
          pointBackgroundColor: "#ffffff",
          pointBorderColor: "#2563eb",
          pointBorderWidth: 2
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false, // uses CSS height from .graph-canvas

      plugins: {
        legend: {
          display: true
        },
        tooltip: {
          // Custom tooltip label
          callbacks: {
            label: (ctx) => ` Wind: ${ctx.raw} mph`
          }
        }
      },

      scales: {
        x: {
          title: {
            display: true,
            text: "Recent Readings"
          },
          ticks: {
            maxRotation: 0,
            autoSkip: true,
            maxTicksLimit: 6
          },
          grid: {
            color: "#e5e7eb"
          }
        },
        y: {
          beginAtZero: true,
          title: {
            display: true,
            text: "Wind Speed (mph)"
          },
          grid: {
            color: "#e5e7eb"
          }
        }
      }
    }
  });
}

// ===================== READ LATEST DATA =====================
// This listens to /buoy/latest in real time.
// Whenever your buoy updates "latest", this callback runs again.
const latestRef = ref(db, "buoy/latest");

onValue(
  latestRef,
  (snapshot) => {
    // snapshot.val() = object stored at /buoy/latest
    const data = snapshot.val();

    // Footer text element (status line at bottom of dashboard)
    const lastUpdatedEl = document.getElementById("lastUpdated");

    // If no data exists yet at that path
    if (!data) {
      if (lastUpdatedEl) lastUpdatedEl.textContent = "No data found at /buoy/latest";
      return;
    }

    // ---------- Fill main dashboard values ----------
    // These IDs match spans/paragraphs in index.html
    const elTemperature = document.getElementById("temperatureF");
    const elHumidity = document.getElementById("humidity");
    const elForecast = document.getElementById("weatherForecast");
    const elWind = document.getElementById("windMph");
    const elGust = document.getElementById("gustMph");
    const elWindDir = document.getElementById("windDirection");
    const elRms = document.getElementById("rms");

    if (elTemperature) elTemperature.textContent = data.temperatureF ?? "—";
    if (elHumidity) elHumidity.textContent = data.humidity ?? "—";
    if (elForecast) elForecast.textContent = data.weatherForecast ?? "—";
    if (elWind) elWind.textContent = data.windMph ?? "—";
    if (elGust) elGust.textContent = data.gustMph ?? "—";
    if (elWindDir) elWindDir.textContent = data.windDirection ?? "—";
    if (elRms) elRms.textContent = data.rms ?? "—";

     // Header date/time (right side of title)
    const date = data.date ?? "";
    const time = data.time ?? "";
    const headerTimeEl = document.getElementById("headerTime");
    const headerDateEl = document.getElementById("headerDate");

    if (headerTimeEl) headerTimeEl.textContent = time;
    if (headerDateEl) headerDateEl.textContent = formatLongDate(date);

    // Buoy status (larger than label)
    const status = data.buoyStatus ?? "—";
    const statusEl = document.getElementById("buoyStatus");
    if (statusEl) {
      statusEl.textContent = status;
      applyStatusStyle(statusEl, status);
    }

    // ---------- Wave Description box ----------
    const statusDescriptionEl = document.getElementById("statusDescription");
    if (statusDescriptionEl) {
      statusDescriptionEl.textContent = getStatusDescription(status);
    }

    // ---------- Forecast emoji ----------
    const forecastIconEl = document.getElementById("forecastIcon");
    if (forecastIconEl) {
      forecastIconEl.textContent = getForecastEmoji(data.weatherForecast ?? "");
    }

    // ---------- Footer ----------
    // This is the local browser time when the update was received
    if (lastUpdatedEl) {
      lastUpdatedEl.textContent = "Live update: " + new Date().toLocaleString();
    }
  },
  (err) => {
    // Error callback if Firebase read fails (rules/path/config issue)
    console.error("Latest read error:", err);

    const lastUpdatedEl = document.getElementById("lastUpdated");
    if (lastUpdatedEl) {
      lastUpdatedEl.textContent = "Firebase error loading latest data (check rules/config).";
    }
  }
);

// ===================== READ HISTORY LOGS =====================
// This listens to /buoy/logs and keeps only the last 12 entries
const historyListEl = document.getElementById("historyList");
const logsQ = query(ref(db, "buoy/logs"), limitToLast(12));

onValue(
  logsQ,
  (snapshot) => {
    // logsObj looks like:
    // {
    //   "-abc123": { ...logData },
    //   "-def456": { ...logData }
    // }
    const logsObj = snapshot.val();

    // If no history exists yet
    if (!logsObj) {
      if (historyListEl) {
        historyListEl.innerHTML = `<div class="history-empty">No history logs found.</div>`;
      }

      // Clear chart if there is no history
      renderWindChart([], []);
      return;
    }

    // Convert object -> array so we can sort and map
    const rows = Object.entries(logsObj).map(([id, v]) => ({ id, ...v }));

    // Sort newest first (for the history list display)
    rows.sort((a, b) => {
      const aDate = toDateObj(a.date, a.time);
      const bDate = toDateObj(b.date, b.time);
      return bDate - aDate;
    });

    // ---------- Build chart data from history rows ----------
    // History list is newest -> oldest, but chart should read left -> right (oldest -> newest)
    // So make a copy, reverse it, then take the most recent 10
    const chartRows = [...rows].reverse().slice(-10);

    // X-axis labels (using time only to keep it short)
    const chartLabels = chartRows.map((r) => r.time ?? "—");

    // Y-axis values = wind speed (convert to numbers in case Firebase stores strings)
    const chartWindValues = chartRows.map((r) => Number(r.windMph) || 0);

    // Create/update chart
    renderWindChart(chartLabels, chartWindValues);

    // ---------- Build history list HTML ----------
    if (historyListEl) {
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
    }
  },
  (err) => {
    // Error callback if history read fails
    console.error("History read error:", err);

    if (historyListEl) {
      historyListEl.innerHTML = `<div class="history-empty">Can't load history logs (check rules).</div>`;
    }
  }
);