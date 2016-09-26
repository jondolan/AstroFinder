var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    xhrRequest("TODO", 'GET', 
      function(responseText) {
        // responseText contains a JSON object with weather info
        var json = JSON.parse(responseText);

        var dictionary = {
          'name': json.name,
          'heading': json.heading
        };
        
        // Send to Pebble
        Pebble.sendAppMessage(dictionary,
          function(e) {
            console.log('Info sent to Pebble successfully!');
          },
          function(e) {
            console.log('Error sending info to Pebble!');
          }
        );
      }      
    );
  }
);