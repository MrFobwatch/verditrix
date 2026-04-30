function getWeather() {
  navigator.geolocation.getCurrentPosition(function(pos) {
    var url = 'https://api.open-meteo.com/v1/forecast' +
      '?latitude='  + pos.coords.latitude +
      '&longitude=' + pos.coords.longitude +
      '&current=temperature_2m,weather_code' +
      '&temperature_unit=celsius';

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.onload = function() {
      if (xhr.status !== 200) {
        console.log('Weather API error: ' + xhr.status);
        return;
      }
      var data;
      try { data = JSON.parse(xhr.responseText); } catch(e) {
        console.log('Weather parse error: ' + e);
        return;
      }
      if (!data.current) { console.log('Weather: unexpected response shape'); return; }
      var temp = Math.round(data.current.temperature_2m);
      var code = data.current.weather_code;

      var condition = 'Clear';
      if      (code === 0)  condition = 'Clear';
      else if (code <= 3)   condition = 'Cloudy';
      else if (code <= 48)  condition = 'Fog';
      else if (code <= 67)  condition = 'Rain';
      else if (code <= 77)  condition = 'Snow';
      else if (code <= 82)  condition = 'Showers';
      else if (code <= 99)  condition = 'T-Storm';

      Pebble.sendAppMessage({
        'TEMPERATURE': temp,
        'CONDITIONS':  condition
      });
    };
    xhr.onerror = function() { console.log('Weather network error'); };
    xhr.send();
  }, function(err) {
    console.log('Location error: ' + err.message);
  });
}

Pebble.addEventListener('ready', function() { getWeather(); });
Pebble.addEventListener('appmessage', function(e) {
  if (e.payload['REQUEST_WEATHER']) getWeather();
});
