const svg = d3.select("#waveform");
const width = 320;
const height = 200;
const margin = { top: 20, right: 20, bottom: 30, left: 40 };
const graphWidth = width - margin.left - margin.right;
const graphHeight = height - margin.top - margin.bottom;

let x_lim = 20;
const y_lim = 100;

// 範囲拡張のための設定
const EXTENSION_THRESHOLD = 1.05; // x軸の105%を超えたら拡張を検討
const EXTENSION_DELAY = 500; // ミリ秒単位での判定遅延
const EXTENSION_AMOUNT = 10; // 一度に増やす量
let extensionTimer = null;
let lastExtensionCheck = 0;

const x = d3.scaleLinear().domain([0, x_lim]).range([0, graphWidth]);
const y = d3.scaleLinear().domain([0, y_lim]).range([graphHeight, 0]);

const graph = svg.append("g")
  .attr("transform", `translate(${margin.left},${margin.top})`);

// x軸とy軸の初期設定
function setupAxes() {
  graph.selectAll("g").remove(); // 既存の軸を削除
  
  graph.append("g")
    .attr("class", "x-axis")
    .attr("transform", `translate(0,${graphHeight})`)
    .call(d3.axisBottom(x));

  graph.append("g")
    .attr("class", "y-axis")
    .call(d3.axisLeft(y));
}

setupAxes();

function extendXAxis() {
  const newXLim = x_lim + EXTENSION_AMOUNT;
  x_lim = newXLim;
  x.domain([0, x_lim]);
  
  // 軸の更新
  graph.select(".x-axis")
    .transition()
    .duration(300)
    .call(d3.axisBottom(x));
    
  // 波形の再描画
  updateWaveform();
}

function checkForExtension(currentX) {
  const currentTime = Date.now();
  
  // 範囲を超えているかチェック
  if (currentX > x_lim * EXTENSION_THRESHOLD) {
    if (!extensionTimer) {
      // タイマーをセット
      extensionTimer = setTimeout(() => {
        extendXAxis();
        extensionTimer = null;
      }, EXTENSION_DELAY);
    }
  } else {
    // 範囲内に戻った場合はタイマーをクリア
    if (extensionTimer) {
      clearTimeout(extensionTimer);
      extensionTimer = null;
    }
  }
  
  lastExtensionCheck = currentTime;
}

let points = [];
let isDragging = false;
let selectedPoint = null;
let animationInterval;

function updateWaveform() {
  graph.selectAll(".line").remove();

  const line = d3.line()
    .x(d => x(d.x))
    .y(d => y(d.y));

  graph.append("path")
    .datum(points)
    .attr("class", "line")
    .attr("fill", "none")
    .attr("stroke", "steelblue")
    .attr("stroke-width", 2)
    .attr("d", line);

  const controlPointGroups = graph.selectAll(".control-point-group")
    .data(points, d => d.id);

  const enterGroups = controlPointGroups.enter()
    .append("g")
    .attr("class", "control-point-group");

  enterGroups.append("circle")
    .attr("class", "touch-area")
    .attr("r", 20)
    .attr("fill", "transparent");

  enterGroups.append("circle")
    .attr("class", "control-point")
    .attr("r", 5)
    .attr("fill", "red");

  controlPointGroups.exit().remove();

  graph.selectAll(".control-point-group")
    .attr("transform", d => `translate(${x(d.x)},${y(d.y)})`)
    .on("touchstart", touchStarted)
    .on("touchmove", touchMoved)
    .on("touchend", touchEnded);
}

function showCoordinates(value_x, value_y, pos_x, pos_y) {
  graph.selectAll(".coordinate-text").remove();
  
  const roundedX = Math.round(value_x);
  const roundedY = Math.round(value_y);
  
  graph.append("text")
    .attr("class", "coordinate-text")
    .attr("x", pos_x)
    .attr("y", pos_y)
    .attr("fill", "black")
    .attr("font-size", "12px")
    .text(`(${roundedX}, ${roundedY})`);
}

function touchStarted(event, d) {
  event.preventDefault();
  selectedPoint = d;
  isDragging = true;
}

function touchMoved(event) {
  event.preventDefault();
  if (!isDragging || !selectedPoint) return;

  const touch = event.touches[0];
  const svgRect = svg.node().getBoundingClientRect();
  const touchX = touch.clientX - svgRect.left - margin.left;
  const touchY = touch.clientY - svgRect.top - margin.top;

  const newX = x.invert(touchX);
  checkForExtension(newX);

  selectedPoint.x = Math.max(0, Math.min(x_lim, newX));
  selectedPoint.y = Math.max(0, Math.min(y_lim, y.invert(touchY)));

  showCoordinates(selectedPoint.x, selectedPoint.y, width - 100, 20);

  points.sort((a, b) => a.x - b.x);
  updateWaveform();
}

function touchEnded(event) {
  event.preventDefault();
  isDragging = false;
  selectedPoint = null;
  
  // タイマーをクリア
  if (extensionTimer) {
    clearTimeout(extensionTimer);
    extensionTimer = null;
  }
}

svg.on("click", function(event) {
  if (isDragging) return;
  const svgRect = svg.node().getBoundingClientRect();
  const mouseX = event.clientX - svgRect.left - margin.left;
  const mouseY = event.clientY - svgRect.top - margin.top;
  
  const newX = x.invert(mouseX);
  
  const newPoint = {
    id: Date.now(),
    x: Math.min(x_lim, newX),
    y: y.invert(mouseY)
  };
  points.push(newPoint);
  points.sort((a, b) => a.x - b.x);
  updateWaveform();
});

function startAnimation() {
  let currentTime = 0;
  const totalDuration = x_lim;
  const fps = 30;
  const interval = 1000 / fps;

  graph.select("#current-position").remove();
  const currentPosition = graph.append("circle")
    .attr("id", "current-position")
    .attr("r", 7);

  animationInterval = setInterval(() => {
    currentTime += interval / 1000;
    if (currentTime > totalDuration) {
      stopAnimation();
      return;
    }

    const currentY = interpolateY(currentTime);
    currentPosition
      .attr("cx", x(currentTime))
      .attr("cy", y(currentY));

  }, interval);
}

function interpolateY(time) {
  if (points.length === 0) return 0;
  if (points.length === 1) return points[0].y;

  let startPoint = points[0];
  let endPoint = points[points.length - 1];

  for (let i = 0; i < points.length - 1; i++) {
    if (time >= points[i].x && time <= points[i + 1].x) {
      startPoint = points[i];
      endPoint = points[i + 1];
      break;
    }
  }

  const t = (time - startPoint.x) / (endPoint.x - startPoint.x);
  return startPoint.y + t * (endPoint.y - startPoint.y);
}

function stopAnimation() {
  clearInterval(animationInterval);
  graph.select("#current-position").remove();
}

document.getElementById("clearPoints").addEventListener("click", function() {
  points = [];
  updateWaveform();
});

document.getElementById("sendToESP32").addEventListener("click", function() {
  const xhr = new XMLHttpRequest();
  xhr.open('PUT', `./get-schedule?points=${JSON.stringify(points)}`);
  xhr.send();
  startAnimation();
});

document.getElementById("stop").addEventListener("click", function() {
  const xhr = new XMLHttpRequest();
  xhr.open('PUT', `./stop`);
  xhr.send();
  stopAnimation();
});

updateWaveform();