const net = require('net');
const { Buffer } = require('buffer');
const { readFileSync } = require('fs');
const { argv } = require('process');

const crc32 = require('buffer-crc32');

// TODO: organize/clean up..

const ErrorCode = {
  SUCCESS: 0,
  STORAGE_FULL: 1,
  CHECKSUM_FAILED: 2,
  REBOOTING: 3,
};

function packRequest(request) {
  const buf = Buffer.alloc(12);
  buf.write('OTA\n');
  buf.writeUInt32LE(request.payloadSize, 4);
  buf.writeUInt32LE(request.checksum || 0, 8);
  return buf;
}

function getResponseStatus(buf) {
  if (buf.toString('utf8', 0, 4) != 'OTA\n') {
    return -1;
  }

  const error = buf.readUInt8(4);

  return error;
}

// TODO: check argv length, print usage

const fileBuffer = readFileSync(argv[3]);
const checksum = crc32.unsigned(fileBuffer);
let allowedRetries = 3;
let payloadSent = false;

console.log('Connecting to', argv[2]);
const socket = new net.Socket();
socket.connect(2222, argv[2], function() {
  console.log('Connected');
  socket.write(packRequest({payloadSize: fileBuffer.length, checksum}));
});

socket.on('data', function(data) {
  const status = getResponseStatus(data);

  switch (status) {
  case ErrorCode.SUCCESS:
    if (payloadSent) {
      console.log('Flashing completed successfully');
      socket.destroy();
    } else {
      console.log('Request approved, sending payload');
      socket.write(fileBuffer);
      payloadSent = true;
    }
    break;
  case ErrorCode.STORAGE_FULL:
    console.log('Failed: storage is full');
    socket.destroy();
    break;
  case ErrorCode.CHECKSUM_FAILED:
    console.log('Checksum failed');
    if (allowedRetries > 0) {
      allowedRetries--;
      console.log('Retrying');
      socket.write(fileBuffer);
    } else {
      socket.destroy();
    }
    break;
  case ErrorCode.REBOOTING:
    // TODO: automatic reconnect
    console.log('Server needs to reboot');
    socket.destroy();
    break;
  default:
    console.log('Uknonwn error code: ' + status);
    socket.destroy();
    break;
  }
});

socket.on('close', function() {
  console.log('Connection closed');
});
