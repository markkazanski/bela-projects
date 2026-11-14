// pepper-grids-clone pattern view (read-only). Open http://bela.local/gui while running.

const DRUM_MAP = [
  [10, 8, 0, 9, 11],
  [15, 7, 13, 12, 6],
  [18, 14, 4, 5, 3],
  [23, 16, 21, 1, 2],
  [24, 19, 17, 20, 22],
];

const PARTS = ["BD", "SD", "HH"];
const N_FLOATS = 114;

function uint32FromFloat(f) {
  const b = new ArrayBuffer(4);
  new Float32Array(b)[0] = f;
  return new Uint32Array(b)[0];
}

function bitAt(words, idx) {
  const w = Math.floor(idx / 32);
  return (words[w] >> (idx & 31)) & 1;
}

let state = null;

function parseBuffer(buf) {
  if (!buf || buf.length < N_FLOATS) return null;
  const levels = buf.slice(0, 96);
  const fireW = [
    uint32FromFloat(buf[96]),
    uint32FromFloat(buf[97]),
    uint32FromFloat(buf[98]),
  ];
  const accW = [
    uint32FromFloat(buf[99]),
    uint32FromFloat(buf[100]),
    uint32FromFloat(buf[101]),
  ];
  const flags = uint32FromFloat(buf[107]);
  return {
    mapX: buf[102],
    mapY: buf[103],
    clockStep: Math.round(buf[104]),
    audibleStep: Math.round(buf[105]),
    freezeStart: Math.round(buf[106]),
    running: (flags & 1) !== 0,
    frozen: (flags & 2) !== 0,
    fill: (flags & 4) !== 0,
    kickDrop: (flags & 8) !== 0,
    square: (flags & 16) !== 0,
    muteBd: (flags & 32) !== 0,
    muteSd: (flags & 64) !== 0,
    muteHh: (flags & 128) !== 0,
    bpm: buf[108],
    swing: buf[109],
    chaos: buf[110],
    density: [buf[111], buf[112], buf[113]],
    levels,
    fireW,
    accW,
  };
}

function setup() {
  createCanvas(windowWidth, Math.max(520, windowHeight));
  textFont("monospace");
}

function windowResized() {
  resizeCanvas(windowWidth, Math.max(520, windowHeight));
}

function draw() {
  background(28, 28, 32);
  const raw = Bela.data.buffers[0];
  const s = parseBuffer(raw);
  if (!s) {
    fill(200);
    textSize(18);
    textAlign(CENTER, CENTER);
    text("Waiting for Bela…", width / 2, height / 2);
    return;
  }
  state = s;

  const margin = 16;
  const topH = 36;
  const gap = 12;
  const stack = width < 720;
  const panelW = stack ? width - 2 * margin : (width - 2 * margin - gap) / 2;
  const mapH = stack ? 220 : height - margin * 2 - topH;
  const patH = stack ? height - mapH - margin * 3 - topH : mapH;
  const mapX = margin;
  const mapY = margin + topH;
  const patX = stack ? margin : margin + panelW + gap;
  const patY = stack ? mapY + mapH + gap : mapY;

  drawReadout(margin, 8, s);
  drawMap(mapX, mapY, panelW, mapH, s);
  drawPattern(patX, patY, panelW, patH, s);
}

function drawReadout(x, y, s) {
  fill(180);
  textSize(13);
  textAlign(LEFT, TOP);
  const run = s.running ? "RUN" : "STOP";
  const frz = s.frozen ? " FREEZE" : "";
  const fillTag = s.fill ? " FILL" : "";
  text(
    `${run}${frz}${fillTag}  BPM ${s.bpm.toFixed(0)}  swing ${(s.swing / 255).toFixed(2)}  chaos ${(s.chaos / 255).toFixed(2)}  dens ${s.density.map((d) => (d / 255).toFixed(2)).join(" ")}`,
    x,
    y,
  );
}

function drawMap(x, y, w, h, s) {
  fill(40);
  noStroke();
  rect(x, y, w, h, 6);
  const side = Math.min(w, h) - 24;
  const ox = x + (w - side) / 2;
  const oy = y + 20;
  const cell = side / 5;
  textSize(11);
  textAlign(CENTER, CENTER);
  for (let j = 0; j < 5; j++) {
    for (let i = 0; i < 5; i++) {
      const cx = ox + i * cell;
      const cy = oy + j * cell;
      fill(55);
      stroke(80);
      strokeWeight(1);
      rect(cx + 2, cy + 2, cell - 4, cell - 4, 4);
      fill(200);
      noStroke();
      text(DRUM_MAP[i][j], cx + cell / 2, cy + cell / 2);
    }
  }
  const mx = ox + cell / 2 + Math.min(4, s.mapX / 64) * cell;
  const my = oy + cell / 2 + Math.min(4, s.mapY / 64) * cell;
  const r = 10;
  noFill();
  stroke(255, 220, 80);
  strokeWeight(s.square ? 3 : 1);
  circle(mx, my, r * 2);
  if (s.square) {
    fill(255, 180, 60, 200);
    noStroke();
    circle(mx, my, r * 1.2);
  }
  fill(140);
  textAlign(LEFT, BOTTOM);
  textSize(12);
  text("5×5 map (node #)", x + 8, y + h - 8);
}

function drawPattern(x, y, w, h, s) {
  fill(40);
  noStroke();
  rect(x, y, w, h, 6);
  const laneH = (h - 50) / 3;
  const stepW = (w - 52) / 32;
  const ox = x + 40;
  const oy = y + 28;
  textSize(12);
  textAlign(LEFT, TOP);
  fill(130);
  text("32-step pattern", x + 8, y + 8);

  for (let p = 0; p < 3; p++) {
    const ly = oy + p * laneH;
    let mute = false;
    if (p === 0 && s.muteBd) mute = true;
    if (p === 1 && s.muteSd) mute = true;
    if (p === 2 && s.muteHh) mute = true;
    fill(mute ? 90 : 200);
    textAlign(RIGHT, CENTER);
    text(PARTS[p], ox - 8, ly + laneH / 2);
    for (let st = 0; st < 32; st++) {
      const idx = p * 32 + st;
      const lvl = s.levels[idx];
      const fires = bitAt(s.fireW, idx);
      const acc = bitAt(s.accW, idx);
      const sx = ox + st * stepW;
      const isHead = st === s.audibleStep;
      let inFreezeLoop = false;
      if (s.frozen) {
        const rel = (st - s.freezeStart + 32) % 32;
        if (rel < 4) inFreezeLoop = true;
      }
      if (st % 8 === 0) {
        stroke(70);
        strokeWeight(1);
        line(sx, ly, sx, ly + laneH - 6);
      }
      const bright = mute ? 0.15 : lvl / 255;
      fill(bright * 220, bright * 180, 60, mute ? 80 : 255);
      noStroke();
      const pad = 1;
      rect(sx + pad, ly + 4, stepW - pad * 2, laneH - 8, 2);
      if (fires && !mute) {
        fill(255, 245, 200);
        rect(sx + pad, ly + 4, stepW - pad * 2, laneH - 8, 2);
      }
      if (acc && !mute) {
        fill(255, 80, 80);
        circle(sx + stepW - 4, ly + 8, 5);
      }
      if (isHead) {
        noFill();
        stroke(255, 255, 255);
        strokeWeight(2);
        rect(sx + 0.5, ly + 2, stepW - 1, laneH - 4, 2);
      }
      if (inFreezeLoop && !isHead) {
        fill(255, 255, 255, 25);
        noStroke();
        rect(sx, ly + 3, stepW, laneH - 6);
      }
    }
  }
  if (s.fill) {
    fill(255, 200, 80);
    textAlign(RIGHT, BOTTOM);
    text("fill (Bar)", x + w - 10, y + h - 8);
  }
}
