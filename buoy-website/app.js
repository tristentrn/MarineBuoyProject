import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-app.js";
//import { getDatabase, ref, onValue } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-database.js";
import { getDatabase, ref, onValue, query, limitToLast } from "https://www.gstatic.com/firebasejs/10.12.5/firebase-database.js";

const firebaseConfig = {
  apiKey: "AIzaSyByUS64PQ41yKp1K003DbhaBJ4E_IM1lyo",
  authDomain: "marine-buoy.firebaseapp.com",
  databaseURL: "https://marine-buoy-default-rtdb.firebaseio.com",
  projectId: "marine-buoy",
  storageBucket: "marine-buoy.firebasestorage.app",
  messagingSenderId: "126380799821",
  appId: "1:126380799821:web:2ad49f7dfe0d3fb2f7a904",
  // measurementId not needed for RTDB reading
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

// ✅ Main page reads only the latest reading
const latestRef = ref(db, "buoy/latest");

onValue(
  latestRef,
  (snapshot) => {
    const data = snapshot.val();
    if (!data) {
      document.getElementById("lastUpdated").textContent = "No data at /buoy/latest yet.";
      return;
    }

    document.getElementById("buoyStatus").textContent = data.buoyStatus ?? "—";
    document.getElementById("weatherForecast").textContent = data.weatherForecast ?? "—";
    document.getElementById("temperatureF").textContent = data.temperatureF ?? "—";
    document.getElementById("humidity").textContent = data.humidity ?? "—";
    document.getElementById("windMph").textContent = data.windMph ?? "—";
    document.getElementById("gustMph").textContent = data.gustMph ?? "—";
    document.getElementById("windDirection").textContent = data.windDirection ?? "—";
    document.getElementById("rms").textContent = data.rms ?? "—";

    const date = data.date ?? "";
    const time = data.time ?? "";
    document.getElementById("timestamp").textContent =
      date && time ? `${date} ${time}` : (date || time || "—");

    document.getElementById("lastUpdated").textContent =
      "Live update: " + new Date().toLocaleString();
  },
  (err) => {
    console.error("Firebase read error:", err);
    document.getElementById("lastUpdated").textContent =
      "Firebase error (likely rules/config). Check console.";
  }
);

// HISTORY: show the last 10 logs under buoy/logs
const historyListEl = document.getElementById("historyList");
const logsQ = query(ref(db, "buoy/logs"), limitToLast(10));

onValue(
  logsQ,
  (snapshot) => {
    const logsObj = snapshot.val();

    if (!historyListEl) return;

    if (!logsObj) {
      historyListEl.innerHTML = `<div class="history-empty">No history yet.</div>`;
      return;
    }

    // Convert {pushId: entry, ...} into array and sort newest -> oldest
    const rows = Object.entries(logsObj).map(([id, v]) => ({ id, ...v }));
    rows.sort((a, b) => {
      const aKey = `${a.date ?? ""} ${a.time ?? ""}`.trim();
      const bKey = `${b.date ?? ""} ${b.time ?? ""}`.trim();
      // reverse sort (newest first); string works because date is YYYY-MM-DD
      return bKey.localeCompare(aKey);
    });

    historyListEl.innerHTML = rows
      .map((r) => {
        const ts = (r.date && r.time) ? `${r.date} ${r.time}` : (r.date || r.time || "—");
        return `
          <div class="history-row">
            <div class="history-left">
               <span class="pill">${r.buoyStatus ?? "—"}</span>
              <span class="kv"><b>F:</b> ${r.weatherForecast ?? "—"}</span>
              <span class="kv"><b>T:</b> ${r.temperatureF ?? "—"}°F</span>
              <span class="kv"><b>H:</b> ${r.humidity ?? "—"}%</span>
              <span class="kv"><b>W:</b> ${r.windMph ?? "—"} mph</span>
              <span class="kv"><b>G:</b> ${r.gustMph ?? "—"} mph</span>
              <span class="kv"><b>Dir:</b> ${r.windDirection ?? "—"}</span>
              <span class="kv"><b>RMS:</b> ${r.rms ?? "—"}</span>
            </div>
            <div class="history-time">${ts}</div>
          </div>
        `;
      })
      .join("");
  },
  (err) => {
    console.error("Logs read error:", err);
    if (historyListEl) {
      historyListEl.innerHTML = `<div class="history-empty">Can't load history (check rules).</div>`;
    }
  }
);