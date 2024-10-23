// ジョイスティックの設定
const joystickOptions = {
  zone: document.getElementById('joystick-zone'),
  mode: 'static',
  position: { left: '50%', top: '50%' },
  color: 'blue',
  size: 150
};

// ジョイスティックの作成
const manager = nipplejs.create(joystickOptions);

// ジョイスティックの情報を表示する要素
const joystickInfo = document.getElementById('joystickInfo');

// ジョイスティックのイベントリスナー
manager.on('move', function (evt, data) {
  const x = data.vector.x;
  const y = data.vector.y;
  const distance = Math.sqrt(x * x + y * y);
  const angle = data.angle.degree;

  joystickInfo.textContent = `X: ${x.toFixed(2)}, Y: ${y.toFixed(2)}, Distance: ${distance.toFixed(2)}, Angle: ${angle.toFixed(2)}°`;

  // ESP32にデータを送信
  const xhr = new XMLHttpRequest();
  xhr.open('PUT', `./get-joystick?joystick=${JSON.stringify({ x, y })}`);
  xhr.send();
});

manager.on('end', function () {
  joystickInfo.textContent = 'Joystick released';
});