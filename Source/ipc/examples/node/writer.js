var IPC = require('../../node/build/Release/IPC');

var conn = new IPC.Connection("ipcdemo", 1);

console.log("Sent: message1");
var msg = new IPC.Message(Buffer.from("Message 1"));
conn.send(msg);

console.log("Sent: message2");
msg = new IPC.Message(Buffer.from("Message 2"));
conn.send(msg);

console.log("Sent: message3");
msg = new IPC.Message(Buffer.from("Message 3"));
conn.send(msg);
