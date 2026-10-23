import type { ScenarioInfo } from "./sim";

const CATEGORY: Record<string, string> = {
  solar_system: "Solar System", solar_system_extended: "Solar System", alpha_centauri: "Stars",
  sgr_a_s2: "Black Holes", binary_black_hole: "Black Holes",
  kepler: "Few-body", figure_eight: "Few-body", pythagorean: "Few-body",
  big_bang: "Cosmology", astronaut_bh: "Black Holes", kepler16_tatooine: "Exoplanets", trappist1: "Exoplanets",
  galilean_moons: "Solar System", jupiter_trojans: "Solar System", pluto_charon: "Solar System",
};
const esc = (s: string) => s.replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]!));

/** Deterministic little orbit sketch per scenario (no image files needed). */
function thumb(name: string): string {
  let h = 2166136261;
  for (const ch of name) h = Math.imul(h ^ ch.charCodeAt(0), 16777619);
  const rnd = () => ((h = Math.imul(h ^ (h >>> 15), 2246822507) >>> 0) / 4294967296);
  const cols = ["#ffd27a", "#6aa9ff", "#e8c98a", "#d4603a", "#9fe0e8"];
  let s = `<svg viewBox="0 0 320 170"><circle cx="160" cy="85" r="7" fill="#ffd27a"/>`;
  const n = 3 + Math.floor(rnd() * 3), rot = rnd() * 180;
  for (let i = 0; i < n; i++) {
    const rx = 36 + i * 30 + rnd() * 12, ry = rx * (0.35 + rnd() * 0.5), a = rnd() * 6.28;
    const x = 160 + rx * Math.cos(a), y = 85 + ry * Math.sin(a);
    s += `<g transform="rotate(${rot + i * 14} 160 85)"><ellipse cx="160" cy="85" rx="${rx}" ry="${ry}" fill="none" stroke="#3a4670" stroke-width="1"/><circle cx="${x}" cy="${y}" r="${3 + rnd() * 2}" fill="${cols[i % 5]}"/></g>`;
  }
  return s + "</svg>";
}

export function renderScenarios(list: ScenarioInfo[]) {
  const cards = document.getElementById("cards")!, chips = document.getElementById("chips")!;
  const search = document.getElementById("search") as HTMLInputElement;
  const cats = ["All", ...new Set(list.map((s) => CATEGORY[s.name] ?? "Other"))];
  let cat = "All";
  const draw = () => {
    const q = search.value.trim().toLowerCase();
    chips.innerHTML = cats.map((c) => `<button class="chip${c === cat ? " on" : ""}" data-c="${esc(c)}">${esc(c)}</button>`).join("");
    cards.innerHTML = list
      .filter((s) => (cat === "All" || (CATEGORY[s.name] ?? "Other") === cat) && (s.title + s.description).toLowerCase().includes(q))
      .map((s) => `<a class="card" href="#/sim/${esc(s.name)}"><div class="th">${thumb(s.name)}</div><div class="tx"><small>${esc(CATEGORY[s.name] ?? "Other")}</small><h3>${esc(s.title)}</h3><p>${esc(s.description)}</p></div></a>`)
      .join("") || `<p class="note">No scenarios match.</p>`;
  };
  chips.onclick = (e) => { const c = (e.target as HTMLElement).dataset.c; if (c) { cat = c; draw(); } };
  search.oninput = draw;
  draw();
}
