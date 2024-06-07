var topic = 'smartdoorlock';

startConnect();

function startConnect() {
  var clientID = "clientID-" + parseInt(Math.random() * 100);
  var host = 'mqtt.eclipseprojects.io';
  var port = 443;

  console.log(host);

  // Ensure the protocol is 'wss' for secure connection
  client = new Paho.MQTT.Client(host, Number(port), "/mqtt", clientID);

  client.onConnectionLost = onConnectionLost;
  client.onMessageArrived = onMessageArrived;

  client.connect({
    useSSL: true,
    onSuccess: onConnect,
    onFailure: function (err) { console.log("Connection failed: " + err.errorMessage); }
  });
}

// Called when the client connects
function onConnect() {
  console.log('Connected to MQTT broker');
  client.subscribe(topic);
  client.subscribe(statusResponseTopic);
  requestDoorStatus();
}

function publishMessage(msg) {
  var message = new Paho.MQTT.Message(msg);
  message.destinationName = topic;
  client.send(message);
}

function requestDoorStatus() {
  var message = "status_request";
  publishMessage(message);
} 

// Called when the client loses its connection
function onConnectionLost(responseObject) {
  if (responseObject.errorCode !== 0) {
    console.error('Connection lost:', responseObject.errorMessage);
  }
}

// Called when a message arrives to toggle the door status
function onMessageArrived(message) {
  var payload = message.payloadString;
  var doorLockBtn = document.getElementById("doorLockBtn");
  
  if (message.destinationName === topic || message.destinationName === statusResponseTopic) {
    if (payload === "locked") {
      doorLockBtn.textContent = "Door Locked";
      doorLockBtn.classList.add("btn-red");
      doorLockBtn.classList.remove("btn-green");
    } else if (payload === "unlocked"){
      doorLockBtn.textContent = "Door Unlocked";
      doorLockBtn.classList.add("btn-green");
      doorLockBtn.classList.remove("btn-red");
    }
  }
}

// Function to toggle the door status when pressing the button
function toggleDoorStatus() {
  var currentStatus = document.getElementById("doorLockBtn").textContent === "Door Locked" ? "locked" : "unlocked";
  var newStatus = currentStatus === "locked" ? "unlocked" : "locked";

  publishMessage(newStatus);
}